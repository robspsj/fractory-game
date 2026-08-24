#include "game_view.hpp"
#include "config.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

static constexpr int MAX_PREVIEW_DEPTH = 100;

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f}, {0.90f, 0.30f, 0.30f}, {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f}, {0.80f, 0.30f, 0.85f},
};

const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_gridBg[3] = {0.25f, 0.40f, 0.60f};
const float GameView::_stationColor[3] = {0.6f, 0.4f, 0.2f};
const float GameView::_reservedInput[3] = {0.2f, 0.6f, 0.3f};
const float GameView::_reservedOutput[3] = {0.6f, 0.2f, 0.2f};
const float GameView::_reservedBuffer[3] = {0.4f, 0.4f, 0.4f};

static uint64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

GameView::GameView(GameModel &model, const Config &cfg)
    : _model(model), _showAnchor(cfg.showAnchor) {
  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize =
      (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    _anchorDepth++;
}

GameView::~GameView() = default;

uint32_t GameView::rgba(const float color[3], float alpha) {
  int r = (int)(color[0] * 255.0f + 0.5f);
  int g = (int)(color[1] * 255.0f + 0.5f);
  int b = (int)(color[2] * 255.0f + 0.5f);
  int a = (int)(alpha * 255.0f + 0.5f);
  if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; if (a > 255) a = 255;
  return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
}

void GameView::addQuad(Rect const r, const float color[3], float alpha) {
  if (r.outsideClip())
    return;
  if (r.w < 0.002f || r.h < 0.002f)
    return;
  if (!_ctx)
    return;
  _lastVertexCount++;
  // rect is in NDC; the context transform maps it to pixels (y-up -> y-down).
  _ctx->fill_rect(BLRect(r.ox, r.oy, r.w, r.h), BLRgba32(rgba(color, alpha)));
}

// Blend a color toward white by t in [0,1], clamped so it never hits pure
// 255 for any channel (avoids white flashes when an icon rasterizes to 1-2px).
static void lighten(const float in[3], float t, float out[3]) {
  for (int i = 0; i < 3; i++)
    out[i] = std::min(in[i] + (1.0f - in[i]) * t, 0.93f);
}

// Draw a vector symbol filled with a radial gradient, plus a soft glow disc
// behind it. Drawn in PIXEL space (transform reset) so the gradient is a true
// circle in screen space — a NDC-space gradient would become an ellipse
// (x scaled by w/2, y by h/2), warping with window size. cx/cy/halfW/halfH in NDC.
void GameView::drawIcon(float cx, float cy, int icon, const float color[3],
                        float halfW, float halfH) {
  if (!_ctx)
    return;
  // NDC -> pixel. target_width/height are the image pixel size.
  const double W = _ctx->target_width();
  const double H = _ctx->target_height();
  const double px = (cx + 1.0) * 0.5 * W;   // x
  const double py = (1.0 - cy) * 0.5 * H;   // y (NDC y-up -> pixel y-down)
  // Pixel radius: keep it circular in screen space. halfW NDC spans W/2 px/unit.
  // halfH spans H/2 px/unit; for a square-ish icon choose min so it fits.
  const double rPx = std::min(halfW * W * 0.5, halfH * H * 0.5);
  if (rPx <= 0.1)
    return;
  _lastVertexCount++;

  // Work in pixel space so radial gradients are true circles.
  _ctx->save();
  _ctx->reset_transform();

  // --- Glow halo: big radial gradient disc fading out ---
  {
    BLGradient glow(BLRadialGradientValues{px, py, 0.0, px, py, rPx * 2.2});
    glow.add_stop(0.0, BLRgba32(rgba(color, 0.35f)));
    glow.add_stop(0.60, BLRgba32(rgba(color, 0.08f)));
    glow.add_stop(1.0, BLRgba32(rgba(color, 0.0f)));
    _ctx->fill_circle(BLCircle(px, py, rPx * 2.2), glow);
  }

  // --- Symbol shape (drawn in pixel coords) ---
  BLPath p;
  const double ux = rPx;  // unit half-width in px
  const double uy = rPx;  // unit half-height in px (circular)
  switch (icon) {
  case 1: { // Heart (point down)
    p.move_to(px, py + uy);
    p.cubic_to(px - ux * 1.8, py + uy * 0.1, px - ux * 1.4, py - uy * 0.9, px, py - uy * 0.35);
    p.cubic_to(px + ux * 1.4, py - uy * 0.9, px + ux * 1.8, py + uy * 0.1, px, py + uy);
    break;
  }
  case 2: { // Star (5-point)
    const int spikes = 5;
    double outer = ux, inner = outer * 0.45;
    for (int i = 0; i < spikes * 2; i++) {
      double ang = i * 3.14159265358979 / spikes - 1.57079632679489;
      double rad = (i % 2 == 0) ? outer : inner;
      double sx = px + std::cos(ang) * rad;
      double sy = py + std::sin(ang) * rad;
      if (i == 0) p.move_to(sx, sy); else p.line_to(sx, sy);
    }
    p.close();
    break;
  }
  case 3: { // Apple: rounded body + brown-ish stem stub
    p.add_circle(BLCircle(px, py + uy * 0.1, rPx * 0.82));
    p.close();
    const float stemCol[3] = {0.45f, 0.30f, 0.12f};
    _ctx->set_stroke_width(std::max(1.0, rPx * 0.22));
    _ctx->stroke_line(px, py + uy * 0.1 - rPx * 0.75, px + ux * 0.2, py - uy * 0.9,
                      BLRgba32(rgba(stemCol, 1.0f)));
    break;
  }
  case 4: { // Triangle (equilateral, up)
    p.move_to(px, py - uy);
    p.line_to(px - ux * 1.1, py + uy * 0.9);
    p.line_to(px + ux * 1.1, py + uy * 0.9);
    p.close();
    break;
  }
  default: { // Diamond
    p.move_to(px, py - uy);
    p.line_to(px + ux, py);
    p.line_to(px, py + uy);
    p.line_to(px - ux, py);
    p.close();
    break;
  }
  }

  // Fill the symbol with a radial gradient (bright core -> element color),
  // centered a bit above the middle for a polished highlight.
  float core[3];
  lighten(color, 0.55f, core);         // never pure white => no flash on tiny icons
  BLGradient sym(BLRadialGradientValues{px, py - rPx * 0.35, 0.0, px, py, rPx * 1.05});
  sym.add_stop(0.0, BLRgba32(rgba(core, 0.95f)));
  sym.add_stop(0.45, BLRgba32(rgba(color, 1.0f)));
  sym.add_stop(1.0, BLRgba32(rgba(color, 0.85f)));
  _ctx->fill_path(p, sym);

  _ctx->restore();
}

void GameView::renderCellItems(float centerX, float centerY, int count,
                               const float color[3], float scale, int icon) {
  if (count <= 0)
    return;
  float itemDotW = 0.05f * scale * _zoom;
  float itemDotH = 0.05f * scale * _aspect * _zoom;
  float spacingX = itemDotW * 2.5f;
  float spacingY = itemDotH * 2.5f;
  auto dot = [&](float cx, float cy) {
    drawIcon(cx, cy, icon, color, itemDotW, itemDotH);
  };
  switch (count) {
  case 1:
    dot(centerX, centerY);
    break;
  case 2:
    dot(centerX - spacingX * 0.5f, centerY);
    dot(centerX + spacingX * 0.5f, centerY);
    break;
  case 3:
    dot(centerX, centerY - spacingY * 0.5f);
    dot(centerX - spacingX * 0.5f, centerY + spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY + spacingY * 0.5f);
    break;
  case 4:
    dot(centerX - spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX - spacingX * 0.5f, centerY + spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY + spacingY * 0.5f);
    break;
  case 5:
    dot(centerX, centerY);
    dot(centerX - spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX - spacingX * 0.5f, centerY + spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY + spacingY * 0.5f);
    break;
  default:
    dot(centerX, centerY);
    break;
  }
}

void GameView::renderEmpty(Rect r, const float bgColor[3]) {
  const float *col = bgColor ? bgColor : _grey;
  addQuad(r, col);
}

void GameView::renderItem(Rect r, int itemId, int count, float scale,
                          const float bgColor[3]) {
  const float *bgCol = bgColor ? bgColor : _grey;
  addQuad(r, bgCol);
  const float *elemCol =
      (itemId >= 0 && itemId < GameModel::ELEMS) ? _elemColors[itemId] : _white;
  int icon = (itemId >= 0) ? itemId % GameModel::ELEMS : 0;
  renderCellItems(r.cx(), r.cy(), count, elemCol, scale, icon);
}

Rect GameView::childCellLayout(int nodeIndex, Rect r, int row, int col) const {
  const Cell& cell = _model.node(nodeIndex);
  int gridDim = cell.content.data.grid.gridDimension;

  float childW = r.w / (gridDim + (gridDim - 1) * _gapRatio);
  float childH = r.h / (gridDim + (gridDim - 1) * _gapRatio);
  float pitchX = childW * (1 + _gapRatio);
  float pitchY = childH * (1 + _gapRatio);
  float halfW = childW * 0.5f;
  float halfH = childH * 0.5f;

  float centerX = r.ox + halfW + col * pitchX;
  float centerY = r.oy + r.h - halfH - row * pitchY;
  return {centerX - halfW, centerY - halfH, childW, childH};
}

void GameView::renderGrid(int nodeIndex, Rect r, int depth, int excludeChild) {
  const Cell &cell = _model.node(nodeIndex);
  int firstChild = cell.content.data.grid.firstChild;
  int gridDim = cell.content.data.grid.gridDimension;

  int absDepth = 0;
  for (int i = nodeIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    absDepth++;
  float grayValue = 0.45f - std::min(absDepth, 10) * 0.03f;
  const float gridBg[3] = {grayValue, grayValue, grayValue + 0.03f};
  addQuad(r, (_showAnchor && depth == 0) ? _gridBg : gridBg);

  for (int row = 0; row < gridDim; row++) {
    for (int col = 0; col < gridDim; col++) {
      int childIndex = firstChild + row * gridDim + col;
      if (childIndex == excludeChild)
        continue;
      Rect child = childCellLayout(nodeIndex, r, row, col);
      renderCell(childIndex, child, depth + 1);
    }
  }
}

void GameView::renderCell(int nodeIndex, Rect r, int depth) {
  int absDepth = 0;
  for (int i = nodeIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    absDepth++;
  if (absDepth >= MAX_PREVIEW_DEPTH)
    return;
  if (r.outsideClip())
    return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize)
    return;

  const Cell &cell = _model.node(nodeIndex);
  const bool isAnchor = _showAnchor && nodeIndex == _anchorIndex;
  switch (cell.content.type) {
  case CellType::EMPTY:
    renderEmpty(r, isAnchor ? _gridBg : nullptr);
    break;
  case CellType::ITEM:
  { float scale = r.w / (_zoom * _cellSize);
    renderItem(r, cell.content.data.item.id, cell.content.data.item.count,
               scale, isAnchor ? _gridBg : nullptr);
    break;
  }
  case CellType::GRID:
    renderGrid(nodeIndex, r, depth);
    break;
  case CellType::STATION:
    addQuad(r, _stationColor);
    break;
  case CellType::RESERVED: {
    const ReservedData &rd = cell.content.data.reserved;
    const float *col = _grey;
    switch (rd.role) {
      case ReserveRole::INPUT:  col = _reservedInput; break;
      case ReserveRole::OUTPUT: col = _reservedOutput; break;
      case ReserveRole::BUFFER: col = _reservedBuffer; break;
      default: break;
    }
    addQuad(r, col);
    if (rd.itemId >= 0) {
      const float *elemCol =
          (rd.itemId >= 0 && rd.itemId < GameModel::ELEMS) ? _elemColors[rd.itemId] : _white;
      renderCellItems(r.cx(), r.cy(), rd.itemCount, elemCol, 1.0f,
                      (rd.itemId >= 0) ? rd.itemId % GameModel::ELEMS : 0);
    }
    break;
  }
  }
}

int GameView::resolveLeafCell(float worldX, float worldY) const {
  constexpr float anchorW = _anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  int leafIdx = -1;

  if (r.contains(worldX, worldY)) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.content.type != CellType::GRID) leafIdx = _anchorIndex;
    else leafIdx = resolveCellAt(worldX, worldY, _anchorIndex, _anchorSize, r);
  } else {
    int currIdx = _anchorIndex;
    Rect currR = r;

    while (true) {
      int parentIdx = _model.node(currIdx).parentId;
      if (parentIdx < 0) break;

      const Cell &parent = _model.node(parentIdx);
      if (parent.content.type != CellType::GRID) break;

      int parentDim = parent.content.data.grid.gridDimension;
      int firstChild = parent.content.data.grid.firstChild;
      int offset = currIdx - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;
      if (row < 0 || row >= parentDim || col < 0 || col >= parentDim) break;

      float k = (float)(parentDim + (parentDim - 1) * _gapRatio);
      float pitchX = currR.w * (1 + _gapRatio);
      float pitchY = currR.h * (1 + _gapRatio);
      Rect parentR = {
        currR.ox - col * pitchX,
        currR.oy + currR.h - currR.h * k + row * pitchY,
        currR.w * k,
        currR.h * k
      };

      if (parentR.contains(worldX, worldY)) {
        leafIdx = resolveCellAt(worldX, worldY, parentIdx, parentDim, parentR);
        break;
      }

      currIdx = parentIdx;
      currR = parentR;
    }
  }

  // Redirect RESERVED cells to their anchor
  if (leafIdx >= 0 && _model.node(leafIdx).content.type == CellType::RESERVED) {
    return _model.node(leafIdx).content.data.reserved.anchorIndex;
  }

  return leafIdx;
}

int GameView::resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim,
                            Rect r) const {
  float childCellSize = r.w / (gridDim + (gridDim - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = r.ox + half;
  float startY = r.oy + r.w - half;

  int col = (int)((worldX - startX) / pitch + 0.5f);
  int row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= gridDim || col < 0 || col >= gridDim)
    return -1;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > half || std::abs(worldY - centerY) > half)
    return -1;

  int childIdx = _model.node(nodeIndex).content.data.grid.firstChild + row * gridDim + col;
  const Cell &child = _model.node(childIdx);

  if (child.content.type == CellType::GRID) {
    int childDim = child.content.data.grid.gridDimension;
    return resolveCellAt(worldX, worldY, childIdx, childDim,
                         {centerX - half, centerY - half, childCellSize, childCellSize});
  }

    return childIdx;
}

int GameView::resolveCenterCell(float worldX, float worldY) const {
  constexpr float anchorW = _anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  if (anchorR.contains(worldX, worldY)) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.content.type != CellType::GRID) return _anchorIndex;
    return resolveCellAtWithSizeCheck(worldX, worldY, _anchorIndex, _anchorSize, anchorR);
  }

  int parentIdx = _model.node(_anchorIndex).parentId;
  if (parentIdx < 0) return _anchorIndex;

  const Cell &parent = _model.node(parentIdx);
  if (parent.content.type != CellType::GRID) return _anchorIndex;

  int parentDim = parent.content.data.grid.gridDimension;
  int firstChild = parent.content.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int anchorRow = offset / parentDim;
  int anchorCol = offset % parentDim;
  if (anchorRow < 0 || anchorRow >= parentDim || anchorCol < 0 || anchorCol >= parentDim)
    return _anchorIndex;

  float parentContentW = anchorW * (parentDim + (parentDim - 1) * _gapRatio);
  Rect child = childCellLayout(parentIdx, {0, 0, parentContentW, parentContentW}, anchorRow, anchorCol);
  Rect parentR = {-anchorW * 0.5f - child.ox, -anchorW * 0.5f - child.oy, parentContentW, parentContentW};

  if (!parentR.contains(worldX, worldY))
    return parentIdx;

  return resolveCellAtWithSizeCheck(worldX, worldY, parentIdx, parentDim, parentR);
}

int GameView::resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex,
                                          int gridDim, Rect r) const {
  const Cell &cell = _model.node(nodeIndex);
  if (cell.content.type != CellType::GRID) return nodeIndex;

  float childCellSize = r.w / (gridDim + (gridDim - 1) * _gapRatio);

  constexpr float screenWidthNDC = 2.0f;
  if (childCellSize * _zoom < MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT * screenWidthNDC)
    return cell.parentId;
  if (childCellSize * _zoom < MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD * screenWidthNDC)
    return nodeIndex;

  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = r.ox + half;
  float startY = r.oy + r.w - half;

  int col = (int)((worldX - startX) / pitch + 0.5f);
  int row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) return nodeIndex;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > half || std::abs(worldY - centerY) > half) return nodeIndex;

  int childIdx = cell.content.data.grid.firstChild + row * gridDim + col;
  const Cell &child = _model.node(childIdx);

  if (child.content.type == CellType::GRID) {
    return resolveCellAtWithSizeCheck(worldX, worldY, childIdx,
                                       child.content.data.grid.gridDimension,
                                       {centerX - half, centerY - half, childCellSize, childCellSize});
  }
  return childIdx;
}

Rect GameView::cellWorldCenter(int targetIdx) const {
  constexpr float anchorW = _anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  int nodeIdx = _anchorIndex;

  while (nodeIdx != targetIdx) {
    const Cell& node = _model.node(nodeIdx);
    if (node.content.type != CellType::GRID) break;

    int gridDim = node.content.data.grid.gridDimension;
    int firstChild = node.content.data.grid.firstChild;

    int directChild = targetIdx;
    while (_model.node(directChild).parentId != nodeIdx) {
      directChild = _model.node(directChild).parentId;
    }

    int offset = directChild - firstChild;
    int row = offset / gridDim;
    int col = offset % gridDim;
    if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) break;

    Rect child = childCellLayout(nodeIdx, r, row, col);

    if (directChild == targetIdx) {
      return child;
    }

    nodeIdx = directChild;
    r = child;
  }

  return r;
}

void GameView::focusTransform(int targetIdx) {
  Rect r = cellWorldCenter(targetIdx);

  constexpr float maxFocusRatio = 100.0f;
  if (r.w * _zoom > maxFocusRatio * 2.0f)
    return;

  const Cell &cell = _model.node(targetIdx);
  _anchorIndex = targetIdx;
  _anchorSize =
      (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    _anchorDepth++;

  _panX = r.cx() * _zoom + _panX;
  _panY = r.cy() * _aspect * _zoom + _panY;
  _zoom = _zoom * r.w / _anchorWidth;
  if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
}

void GameView::resetView() {
  _anchorIndex = 0;
  const Cell &cell = _model.node(0);
  _anchorSize =
      (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  _zoom = 1.0f;
  _panX = 0.0f;
  _panY = 0.0f;
}

void GameView::focusCenterCell(int winW, int winH) {
  float worldX, worldY;
  screenToWorld(winW / 2, winH / 2, winW, winH, worldX, worldY);
  int idx = resolveCenterCell(worldX, worldY);
  if (idx < 0 || idx == _anchorIndex) return;

  if (isDescendant(_anchorIndex, idx)) {
    focusTransform(idx);
    return;
  }

  while (!isDescendant(_anchorIndex, idx)) {
    if (!unfocusOneLevel()) return;
  }
  focusTransform(idx);
}

bool GameView::unfocusOneLevel() {
  int parentIdx = _model.node(_anchorIndex).parentId;
  if (parentIdx < 0) return false;

  const Cell &parent = _model.node(parentIdx);
  if (parent.content.type != CellType::GRID) return false;

  int gridDim = parent.content.data.grid.gridDimension;
  int firstChild = parent.content.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int row = offset / gridDim;
  int col = offset % gridDim;

  constexpr float anchorW = _anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  Rect child = childCellLayout(parentIdx, anchorR, row, col);

  float centerX = child.cx();
  float centerY = child.cy();
  float savedZoom = _zoom;

  _anchorIndex = parentIdx;
  _anchorSize = gridDim;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    _anchorDepth++;
  _zoom = savedZoom * _anchorWidth / child.w;
  if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
  _panX -= centerX * (savedZoom * _anchorWidth / child.w);
  _panY -= centerY * _aspect * (savedZoom * _anchorWidth / child.w);
  return true;
}

bool GameView::isDescendant(int ancestor, int node) const {
  while (node >= 0) {
    if (node == ancestor) return true;
    node = _model.node(node).parentId;
  }
  return false;
}

void GameView::renderAnchor(const int anchorIndex, const Rect r, const int depth, const int excludeChild) {
  const Cell &cell = _model.node(anchorIndex);
  bool coversScreen = r.ox <= -1.0f && r.oy <= -1.0f &&
                      r.ox + r.w >= 1.0f && r.oy + r.h >= 1.0f;
  if (!coversScreen && cell.parentId >= 0) {
    const Cell &parent = _model.node(cell.parentId);
    if (parent.content.type == CellType::GRID) {
      int parentDim = parent.content.data.grid.gridDimension;
      int firstChild = parent.content.data.grid.firstChild;
      int offset = anchorIndex - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;

      float k = (float)(parentDim + (parentDim - 1) * _gapRatio);
      float pitchX = r.w * (1 + _gapRatio);
      float pitchY = r.h * (1 + _gapRatio);

      Rect parentR = {
        r.ox - col * pitchX,
        r.oy + r.h - r.h * k + row * pitchY,
        r.w * k,
        r.h * k
      };
      renderAnchor(cell.parentId, parentR, depth + 1,
                   excludeChild >= 0 ? excludeChild : anchorIndex);
    }
  }

  if (depth >= MAX_PREVIEW_DEPTH)
    return;
  if (r.outsideClip())
    return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize)
    return;

  if (cell.content.type == CellType::GRID)
    renderGrid(anchorIndex, r, depth, excludeChild);
  else
    renderCell(anchorIndex, r, depth);
}

void GameView::render(int winW, int winH) {
  _aspect = (float)winW / (float)winH;
  _lastVertexCount = 0;

  if (_image.width() != winW || _image.height() != winH || _image.format() != BL_FORMAT_XRGB32) {
    _image.create(winW, winH, BL_FORMAT_XRGB32);
  }

  BLContext ctx(_image);
  _ctx = &ctx;
  // Fully replace the frame with the background colour each pass, then alpha-blend
  // the cells/icons/(DRAG glow) on top so gradients+glow composite correctly.
  ctx.set_comp_op(BL_COMP_OP_SRC_COPY);
  ctx.fill_all(BLRgba32(0x1A, 0x1A, 0x26));  // background
  ctx.set_comp_op(BL_COMP_OP_SRC_OVER);

  // NDC -> pixel transform: px = (nx+1)*w/2, py = (1-ny)*h/2
  ctx.set_transform(BLMatrix2D(winW * 0.5, 0.0, 0.0, -winH * 0.5, winW * 0.5, winH * 0.5));

  uint64_t t0 = nowMs();

  constexpr float anchorW = _anchorWidth;
  float ox = -anchorW * 0.5f * _zoom + _panX;
  float oy = -anchorW * 0.5f * _aspect * _zoom + _panY;
  float cw = anchorW * _zoom;
  float ch = anchorW * _aspect * _zoom;

  renderAnchor(_anchorIndex, {ox, oy, cw, ch}, 0);

  if (_model.hasDrag()) {
    int dragId = _model.dragItemId();
    int dragAmount = _model.dragAmount();

    if (!_dragWasActive) {
      _dragAnimStartMs = nowMs();
    }

    double elapsed = (double)(nowMs() - _dragAnimStartMs) / 1000.0;
    double t = elapsed / _dragAnimDuration;
    if (t >= 1.0) {
      t = 1.0;
    }
    float eased = (float)(t * (2.0 - t));

    float scale = (0.5f + 0.5f * eased) / _zoom;

    float sx = _dragWX * _zoom + _panX;
    float sy = _dragWY * _aspect * _zoom + _panY;
    const float *elemCol = (dragId >= 0 && dragId < GameModel::ELEMS) ? _elemColors[dragId] : _white;
    int dragIcon = (dragId >= 0) ? dragId % GameModel::ELEMS : 0;
    renderCellItems(sx, sy, dragAmount, elemCol, scale, dragIcon);
  }
  _dragWasActive = _model.hasDrag();

  uint64_t t1 = nowMs();
  _lastGenMs = (float)(t1 - t0);

  ctx.end();
  _ctx = nullptr;
  _lastDrawMs = (float)(nowMs() - t1);
}

bool GameView::screenToGrid(int pixelX, int pixelY, int winW, int winH, int &row,
                            int &col) const {
  float worldX, worldY;
  screenToWorld(pixelX, pixelY, winW, winH, worldX, worldY);

  int size = _anchorSize;

  float childCellSize = _anchorWidth / (size + (size - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = size * pitch;
  float halfContent = childCellSize * 0.5f;

  float startX = -totalSize * 0.5f + pitch * 0.5f;
  float startY = totalSize * 0.5f - pitch * 0.5f;

  col = (int)((worldX - startX) / pitch + 0.5f);
  row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= size || col < 0 || col >= size)
    return false;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > halfContent ||
      std::abs(worldY - centerY) > halfContent)
    return false;

  return true;
}

void GameView::screenToWorld(int pixelX, int pixelY, int winW, int winH, float &worldX,
                             float &worldY) const {
  float aspect = (float)winW / (float)winH;
  worldX = ((2.0f * pixelX / (float)winW) - 1.0f - _panX) / _zoom;
  worldY = ((1.0f - (2.0f * pixelY / (float)winH)) - _panY) / aspect / _zoom;
}

float GameView::gridToWorldX(int col) const {
  float childCellSize = _anchorWidth / (_anchorSize + (_anchorSize - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = _anchorSize * pitch;
  return -totalSize * 0.5f + pitch * 0.5f + col * pitch;
}

float GameView::gridToWorldY(int row) const {
  float childCellSize = _anchorWidth / (_anchorSize + (_anchorSize - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = _anchorSize * pitch;
  return totalSize * 0.5f - pitch * 0.5f - row * pitch;
}

void GameView::zoom(float factor, float mouseNX, float mouseNY) {
  float oldZoom = _zoom;
  _zoom *= factor;
  if (_zoom < MIN_ZOOM)
    _zoom = MIN_ZOOM;
  float actualFactor = _zoom / oldZoom;
  _panX = mouseNX * (1.0f - actualFactor) + _panX * actualFactor;
  _panY = mouseNY * (1.0f - actualFactor) + _panY * actualFactor;
}

void GameView::startPan(int pixelX, int pixelY) {
  _isPanning = true;
  _lastPanX = pixelX;
  _lastPanY = pixelY;
}

void GameView::continuePan(int pixelX, int pixelY, int winW, int winH) {
  float deltaX = 2.0f * (pixelX - _lastPanX) / (float)winW;
  float deltaY = -2.0f * (pixelY - _lastPanY) / (float)winH;
  _panX += deltaX;
  _panY += deltaY;
  _lastPanX = pixelX;
  _lastPanY = pixelY;
}

void GameView::endPan() { _isPanning = false; }