#include "cell_renderer.hpp"
#include "game_view.hpp"
#include "layout_engine.hpp"
#include "icon_renderer.hpp"
#include "badge_renderer.hpp"
#include "ui_library.hpp"
#include <algorithm>
#include "include/core/SkCanvas.h"
#include "include/core/SkRRect.h"

void CellRenderer::addQuad(GameView &view, Rect r, const float color[3], float alpha) {
  if (r.outsideClip()) return;
  if (r.w < 0.002f || r.h < 0.002f) return;
  if (!view._canvas) return;
  view._lastVertexCount++;
  view._quadPaint.setColor(IconRenderer::rgba(color, alpha));
  view._quadPaint.setAntiAlias(true);
  view._canvas->drawRect(SkRect::MakeXYWH(r.ox, r.oy, r.w, r.h), view._quadPaint);
}

void CellRenderer::renderEmpty(GameView &view, Rect r, const float bgColor[3]) {
  const float *col = bgColor ? bgColor : view._grey;
  addQuad(view, r, col);
}

void CellRenderer::renderItem(GameView &view, Rect r, int itemId, int count, float scale, const float bgColor[3], int variation, int nodeIndex) {
#if !defined(__EMSCRIPTEN__)
  if (view._ui) {
    int vi = std::clamp(variation - 1, 0, UiLib::kFamilies - 1);
    if (view._ui->cell[vi]) renderTile(view._canvas, view._ui->cell[vi], r);
  } else
#endif
  {
    const float *bgCol = bgColor ? bgColor : view._grey;
    addQuad(view, r, bgCol);
  }
  const GameView::ItemRenderInfo *info = nullptr;
  auto it = view._itemRenderInfo.find(itemId);
  if (it != view._itemRenderInfo.end()) info = &it->second;
  else if (itemId >= 0 && itemId < GameModel::ELEMS) {
    const float *legacyCol = view._elemColors[itemId];
    static GameView::ItemRenderInfo tmp;
    tmp.color[0] = legacyCol[0]; tmp.color[1] = legacyCol[1]; tmp.color[2] = legacyCol[2];
    tmp.icon = itemId % GameModel::ELEMS;
    tmp.svgPath.clear();
    info = &tmp;
  }
  const float *elemCol = info ? info->color : view._white;
  IconRenderer::renderCellItems(view, r.cx(), r.cy(), count, elemCol, scale, itemId, nodeIndex);
  if (count >= 5) BadgeRenderer::drawCountBadge(view, r, count);
}

void CellRenderer::renderGrid(GameView &view, int nodeIndex, Rect r, int depth, int excludeChild) {
  const Cell &cell = view._model.node(nodeIndex);
  int firstChild = cell.content.data.grid.firstChild;
  int gridDim = cell.content.data.grid.gridDimension;
  int absDepth = 0;
  for (int i = nodeIndex; view._model.node(i).parentId >= 0; i = view._model.node(i).parentId) absDepth++;
  // Mark grid container as visible and cache NDC (once per frame)
  if (!r.outsideClip() && r.w >= 0.002f && r.h >= 0.002f) {
    view.setVisible(nodeIndex);
    view.setNdc(nodeIndex, r.cx(), r.cy(), r.halfW());
  }
  float grayValue = 0.45f - std::min(absDepth, 10) * 0.03f;
  const float gridBg[3] = {grayValue, grayValue, grayValue + 0.03f};
#if !defined(__EMSCRIPTEN__)
  if (view._ui) {
    int vi = std::clamp(cell.content.data.grid.variation - 1, 0, UiLib::kFamilies - 1);
    if (view._ui->bg[vi]) renderTile(view._canvas, view._ui->bg[vi], r);
  } else
#endif
  addQuad(view, r, (view._showAnchor && depth == 0) ? view._gridBg : gridBg);
  for (int row = 0; row < gridDim; row++) {
    for (int col = 0; col < gridDim; col++) {
      int childIndex = firstChild + row * gridDim + col;
      if (childIndex == excludeChild) continue;
      Rect child = LayoutEngine::childCellLayout(view, nodeIndex, r, row, col);
      renderCell(view, childIndex, child, depth + 1);
    }
  }
}

void CellRenderer::renderCell(GameView &view, int nodeIndex, Rect r, int depth) {
  int absDepth = 0;
  for (int i = nodeIndex; view._model.node(i).parentId >= 0; i = view._model.node(i).parentId) absDepth++;
  if (absDepth >= 100) return;
  if (r.outsideClip()) return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize) return;
  view._bench.cellN++;
  view.setVisible(nodeIndex);
  view.setNdc(nodeIndex, r.cx(), r.cy(), r.halfW());
  const Cell &cell = view._model.node(nodeIndex);
  const bool isAnchor = view._showAnchor && nodeIndex == view._anchorIndex;
  switch (cell.content.type) {
  case CellType::EMPTY:
#if !defined(__EMSCRIPTEN__)
    if (view._ui) {
      int vi = std::clamp(LayoutEngine::variationOf(view, nodeIndex) - 1, 0, UiLib::kFamilies - 1);
      if (view._ui->cell[vi]) renderTile(view._canvas, view._ui->cell[vi], r);
      else renderEmpty(view, r, isAnchor ? view._gridBg : nullptr);
    } else
#endif
    renderEmpty(view, r, isAnchor ? view._gridBg : nullptr);
    break;
  case CellType::ITEM:
  { float scale = r.w / (view._zoom * view._cellSize);
    renderItem(view, r, cell.content.data.item.id, cell.content.data.item.count, scale, isAnchor ? view._gridBg : nullptr, LayoutEngine::variationOf(view, nodeIndex), nodeIndex);
    break;
  }
  case CellType::GRID:
    renderGrid(view, nodeIndex, r, depth);
    break;
  case CellType::STATION:
    addQuad(view, r, view._stationColor);
    break;
  case CellType::RESERVED: {
    const ReservedData &rd = cell.content.data.reserved;
    const float *col = view._grey;
    switch (rd.role) {
      case ReserveRole::INPUT:  col = view._reservedInput; break;
      case ReserveRole::OUTPUT: col = view._reservedOutput; break;
      case ReserveRole::BUFFER: col = view._reservedBuffer; break;
      default: break;
    }
    addQuad(view, r, col);
    if (rd.itemId >= 0) {
      const float *elemCol = (rd.itemId >= 0 && rd.itemId < GameModel::ELEMS) ? view._elemColors[rd.itemId] : view._white;
      IconRenderer::renderCellItems(view, r.cx(), r.cy(), rd.itemCount, elemCol, 1.0f, rd.itemId, nodeIndex);
    }
    break;
  }
  }
  if (nodeIndex == view._hoverCellNode) {
    static const float kHoverTint[3] = {0.85f, 0.86f, 0.90f};
    const SkScalar radX = (SkScalar)(r.w * (10.0f / 64.0f));
    const SkScalar radY = (SkScalar)(r.h * (10.0f / 64.0f));
    SkRRect rr;
    rr.setRectXY(SkRect::MakeXYWH((SkScalar)r.ox, (SkScalar)r.oy, (SkScalar)r.w, (SkScalar)r.h), radX, radY);
    SkPaint p; p.setAntiAlias(true); p.setColor(IconRenderer::rgba(kHoverTint, 0.18f));
    view._canvas->drawRRect(rr, p);
  }
}

void CellRenderer::renderAnchor(GameView &view, int anchorIndex, Rect r, int depth, int excludeChild) {
  const Cell &cell = view._model.node(anchorIndex);
  bool coversScreen = r.ox <= -1.0f && r.oy <= -1.0f && r.ox + r.w >= 1.0f && r.oy + r.h >= 1.0f;
  if (!coversScreen && cell.parentId >= 0) {
    const Cell &parent = view._model.node(cell.parentId);
    if (parent.content.type == CellType::GRID) {
      int parentDim = parent.content.data.grid.gridDimension;
      int firstChild = parent.content.data.grid.firstChild;
      int offset = anchorIndex - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;
      float k = (float)(parentDim + (parentDim - 1) * view._gapRatio);
      float pitchX = r.w * (1 + view._gapRatio);
      float pitchY = r.h * (1 + view._gapRatio);
      Rect parentR = { r.ox - col * pitchX, r.oy + r.h - r.h * k + row * pitchY, r.w * k, r.h * k };
      renderAnchor(view, cell.parentId, parentR, depth + 1, excludeChild >= 0 ? excludeChild : anchorIndex);
    }
  }
  if (depth >= 100) return;
  if (r.outsideClip()) return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize) return;
  // Mark anchor node itself as visible and cache NDC
  view.setVisible(anchorIndex);
  view.setNdc(anchorIndex, r.cx(), r.cy(), r.halfW());
  if (cell.content.type == CellType::GRID) renderGrid(view, anchorIndex, r, depth, excludeChild);
  else renderCell(view, anchorIndex, r, depth);
}
