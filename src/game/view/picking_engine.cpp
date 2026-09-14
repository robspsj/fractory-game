#include "picking_engine.hpp"
#include "game_view.hpp"
#include "layout_engine.hpp"
#include <cmath>

int PickingEngine::resolveCellAt(const GameView &view, float worldX, float worldY, int nodeIndex,
                                 int gridDim, Rect r) {
  float childCellSize = r.w / (gridDim + (gridDim - 1) * view._gapRatio);
  float pitch = childCellSize * (1 + view._gapRatio);
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

  int childIdx = view._model.node(nodeIndex).content.data.grid.firstChild + row * gridDim + col;
  const Cell &child = view._model.node(childIdx);

  if (view._model.typeOf(childIdx) == CellType::GRID) {
    int childDim = child.content.data.grid.gridDimension;
    return resolveCellAt(view, worldX, worldY, childIdx, childDim,
                         {centerX - half, centerY - half, childCellSize, childCellSize});
  }

  return childIdx;
}

int PickingEngine::resolveCellAtWithSizeCheck(const GameView &view, float worldX, float worldY,
                                              int nodeIndex, int gridDim, Rect r) {
  const Cell &cell = view._model.node(nodeIndex);
  if (view._model.typeOf(nodeIndex) != CellType::GRID) return nodeIndex;

  float childCellSize = r.w / (gridDim + (gridDim - 1) * view._gapRatio);

  constexpr float screenWidthNDC = 2.0f;
  if (childCellSize * view._zoom < MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT * screenWidthNDC)
    return cell.parentId;
  if (childCellSize * view._zoom < MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD * screenWidthNDC)
    return nodeIndex;

  float pitch = childCellSize * (1 + view._gapRatio);
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
  const Cell &child = view._model.node(childIdx);

  if (view._model.typeOf(childIdx) == CellType::GRID) {
    return resolveCellAtWithSizeCheck(view, worldX, worldY, childIdx,
                                      child.content.data.grid.gridDimension,
                                      {centerX - half, centerY - half, childCellSize, childCellSize});
  }
  return childIdx;
}

int PickingEngine::resolveCenterCell(const GameView &view, float worldX, float worldY) {
  const float anchorW = GameView::_anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  if (anchorR.contains(worldX, worldY)) {
    const Cell &anchor = view._model.node(view._anchorIndex);
    if (view._model.typeOf(view._anchorIndex) != CellType::GRID) return view._anchorIndex;
    return resolveCellAtWithSizeCheck(view, worldX, worldY, view._anchorIndex, view._anchorSize, anchorR);
  }

  int parentIdx = view._model.node(view._anchorIndex).parentId;
  if (parentIdx < 0) return view._anchorIndex;

  const Cell &parent = view._model.node(parentIdx);
  if (view._model.typeOf(parentIdx) != CellType::GRID) return view._anchorIndex;

  int parentDim = parent.content.data.grid.gridDimension;
  int firstChild = parent.content.data.grid.firstChild;
  int offset = view._anchorIndex - firstChild;
  int anchorRow = offset / parentDim;
  int anchorCol = offset % parentDim;
  if (anchorRow < 0 || anchorRow >= parentDim || anchorCol < 0 || anchorCol >= parentDim)
    return view._anchorIndex;

  float parentContentW = anchorW * (parentDim + (parentDim - 1) * view._gapRatio);
  Rect child = LayoutEngine::childCellLayout(view, parentIdx, {0, 0, parentContentW, parentContentW}, anchorRow, anchorCol);
  Rect parentR = {-anchorW * 0.5f - child.ox, -anchorW * 0.5f - child.oy, parentContentW, parentContentW};

  if (!parentR.contains(worldX, worldY))
    return parentIdx;

  return resolveCellAtWithSizeCheck(view, worldX, worldY, parentIdx, parentDim, parentR);
}

int PickingEngine::resolveLeafCell(const GameView &view, float worldX, float worldY) {
  const float anchorW = GameView::_anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  int leafIdx = -1;

  if (r.contains(worldX, worldY)) {
    const Cell &anchor = view._model.node(view._anchorIndex);
    if (view._model.typeOf(view._anchorIndex) != CellType::GRID) leafIdx = view._anchorIndex;
    else leafIdx = resolveCellAt(view, worldX, worldY, view._anchorIndex, view._anchorSize, r);
  } else {
    int currIdx = view._anchorIndex;
    Rect currR = r;

    while (true) {
      int parentIdx = view._model.node(currIdx).parentId;
      if (parentIdx < 0) break;

      const Cell &parent = view._model.node(parentIdx);
      if (view._model.typeOf(parentIdx) != CellType::GRID) break;

      int parentDim = parent.content.data.grid.gridDimension;
      int firstChild = parent.content.data.grid.firstChild;
      int offset = currIdx - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;
      if (row < 0 || row >= parentDim || col < 0 || col >= parentDim) break;

      float k = (float)(parentDim + (parentDim - 1) * view._gapRatio);
      float pitchX = currR.w * (1 + view._gapRatio);
      float pitchY = currR.h * (1 + view._gapRatio);
      Rect parentR = {
        currR.ox - col * pitchX,
        currR.oy + currR.h - currR.h * k + row * pitchY,
        currR.w * k,
        currR.h * k
      };

      if (parentR.contains(worldX, worldY)) {
        leafIdx = resolveCellAt(view, worldX, worldY, parentIdx, parentDim, parentR);
        break;
      }

      currIdx = parentIdx;
      currR = parentR;
    }
  }

  if (leafIdx >= 0 && view._model.typeOf(leafIdx) == CellType::RESERVED) {
    return leafIdx - view._model.node(leafIdx).content.data.reserved.anchorDelta;
  }

  return leafIdx;
}
