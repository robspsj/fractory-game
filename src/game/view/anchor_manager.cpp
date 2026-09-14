#include "anchor_manager.hpp"
#include "game_view.hpp"
#include "layout_engine.hpp"
#include "picking_engine.hpp"

void AnchorManager::focusTransform(GameView &view, int targetIdx) {
  Rect r = LayoutEngine::cellWorldCenter(view, targetIdx);

  constexpr float maxFocusRatio = 100.0f;
  if (r.w * view._zoom > maxFocusRatio * 2.0f)
    return;

  const Cell &cell = view._model.node(targetIdx);
  view._anchorIndex = targetIdx;
  view._anchorSize =
      (view._model.typeOf(targetIdx) == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  view._anchorDepth = 0;
  for (int i = view._anchorIndex; view._model.node(i).parentId >= 0; i = view._model.node(i).parentId)
    view._anchorDepth++;

  view._panX = r.cx() * view._zoom + view._panX;
  view._panY = r.cy() * view._aspect * view._zoom + view._panY;
  view._zoom = view._zoom * r.w / view._anchorWidth;
  if (view._zoom < MIN_ZOOM) view._zoom = MIN_ZOOM;
}

void AnchorManager::resetView(GameView &view) {
  view._anchorIndex = 0;
  const Cell &cell = view._model.node(0);
  view._anchorSize =
      (view._model.typeOf(0) == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  view._anchorDepth = 0;
  view._zoom = view._fitZoom;
  view._panX = 0.0f;
  view._panY = 0.0f;
}

void AnchorManager::focusCenterCell(GameView &view, int winW, int winH) {
  float worldX, worldY;
  // Use CoordinateMapper logic inline to avoid circular include
  float aspect = (float)winW / (float)winH;
  worldX = ((2.0f * (winW / 2) / (float)winW) - 1.0f - view._panX) / view._zoom;
  worldY = ((1.0f - (2.0f * (winH / 2) / (float)winH)) - view._panY) / aspect / view._zoom;
  int idx = PickingEngine::resolveCenterCell(view, worldX, worldY);
  if (idx < 0 || idx == view._anchorIndex) return;

  if (LayoutEngine::isDescendant(view, view._anchorIndex, idx)) {
    focusTransform(view, idx);
    return;
  }

  while (!LayoutEngine::isDescendant(view, view._anchorIndex, idx)) {
    if (!unfocusOneLevel(view)) return;
  }
  focusTransform(view, idx);
}

bool AnchorManager::unfocusOneLevel(GameView &view) {
  int parentIdx = view._model.node(view._anchorIndex).parentId;
  if (parentIdx < 0) return false;

  const Cell &parent = view._model.node(parentIdx);
  if (view._model.typeOf(parentIdx) != CellType::GRID) return false;

  int gridDim = parent.content.data.grid.gridDimension;
  int firstChild = parent.content.data.grid.firstChild;
  int offset = view._anchorIndex - firstChild;
  int row = offset / gridDim;
  int col = offset % gridDim;

  const float anchorW = GameView::_anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  Rect child = LayoutEngine::childCellLayout(view, parentIdx, anchorR, row, col);

  float centerX = child.cx();
  float centerY = child.cy();
  float savedZoom = view._zoom;

  view._anchorIndex = parentIdx;
  view._anchorSize = gridDim;
  view._anchorDepth = 0;
  for (int i = view._anchorIndex; view._model.node(i).parentId >= 0; i = view._model.node(i).parentId)
    view._anchorDepth++;
  view._zoom = savedZoom * view._anchorWidth / child.w;
  if (view._zoom < MIN_ZOOM) view._zoom = MIN_ZOOM;
  view._panX -= centerX * (savedZoom * view._anchorWidth / child.w);
  view._panY -= centerY * view._aspect * (savedZoom * view._anchorWidth / child.w);
  return true;
}
