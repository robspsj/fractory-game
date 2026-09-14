#include "layout_engine.hpp"
#include "game_view.hpp"
#include <cmath>

Rect LayoutEngine::childCellLayout(const GameView &view, int nodeIndex, Rect r, int row, int col) {
  const Cell& cell = view._model.node(nodeIndex);
  int gridDim = cell.content.data.grid.gridDimension;

  float childW = r.w / (gridDim + (gridDim - 1) * view._gapRatio);
  float childH = r.h / (gridDim + (gridDim - 1) * view._gapRatio);
  float pitchX = childW * (1 + view._gapRatio);
  float pitchY = childH * (1 + view._gapRatio);
  float halfW = childW * 0.5f;
  float halfH = childH * 0.5f;

  float centerX = r.ox + halfW + col * pitchX;
  float centerY = r.oy + r.h - halfH - row * pitchY;
  return {centerX - halfW, centerY - halfH, childW, childH};
}

Rect LayoutEngine::cellWorldCenter(const GameView &view, int targetIdx) {
  const float anchorW = GameView::_anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  int nodeIdx = view._anchorIndex;

  while (nodeIdx != targetIdx) {
    const Cell& node = view._model.node(nodeIdx);
    if (view._model.typeOf(nodeIdx) != CellType::GRID) break;

    int gridDim = node.content.data.grid.gridDimension;
    int firstChild = node.content.data.grid.firstChild;

    int directChild = targetIdx;
    while (view._model.node(directChild).parentId != nodeIdx) {
      directChild = view._model.node(directChild).parentId;
    }

    int offset = directChild - firstChild;
    int row = offset / gridDim;
    int col = offset % gridDim;
    if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) break;

    Rect child = childCellLayout(view, nodeIdx, r, row, col);

    if (directChild == targetIdx) {
      return child;
    }

    nodeIdx = directChild;
    r = child;
  }

  return r;
}

static Rect upLayout(const Rect &child, int parentDim, float gapRatio, int row, int col) {
  float k = (float)(parentDim + (parentDim - 1) * gapRatio);
  float pitchX = child.w * (1 + gapRatio);
  float pitchY = child.h * (1 + gapRatio);
  return {child.ox - col * pitchX,
          child.oy + child.h - child.h * k + row * pitchY,
          child.w * k,
          child.h * k};
}

bool LayoutEngine::cellCenterNDC(const GameView &view, int nodeIndex, float &nx, float &ny, float &halfNDC) {
  if (nodeIndex < 0 || nodeIndex >= view._model.totalNodes()) return false;
  Rect R = {-view._anchorWidth * 0.5f * view._zoom + view._panX,
            -view._anchorWidth * 0.5f * view._aspect * view._zoom + view._panY,
            view._anchorWidth * view._zoom,
            view._anchorWidth * view._aspect * view._zoom};

  auto depthOf = [&](int idx) {
    int d = 0;
    while (view._model.node(idx).parentId >= 0) { idx = view._model.node(idx).parentId; d++; }
    return d;
  };
  auto hopInParent = [&](int child, int &parent, int &row, int &col) {
    const int p = view._model.node(child).parentId;
    if (p < 0) return false;
    const Cell &pc = view._model.node(p);
    if (view._model.typeOf(p) != CellType::GRID) return false;
    int dim = pc.content.data.grid.gridDimension;
    int first = pc.content.data.grid.firstChild;
    int off = child - first;
    if (off < 0 || off >= dim * dim) return false;
    parent = p; row = off / dim; col = off % dim;
    return true;
  };

  int a = view._anchorIndex, b = nodeIndex;
  int da = depthOf(a), db = depthOf(b);
  struct Hop { int parent, row, col; };
  Hop A[128], B[128]; int nA = 0, nB = 0;
  int p, r, c;
  while (da > db) { if (!hopInParent(a, p, r, c)) return false; if (nA >= 128) return false; A[nA++] = {p, r, c}; a = p; da--; }
  while (db > da) { if (!hopInParent(b, p, r, c)) return false; if (nB >= 128) return false; B[nB++] = {p, r, c}; b = p; db--; }
  while (a != b) {
    if (nA >= 128 || nB >= 128) return false;
    if (!hopInParent(a, p, r, c)) return false; A[nA++] = {p, r, c}; a = p;
    if (!hopInParent(b, p, r, c)) return false; B[nB++] = {p, r, c}; b = p;
  }
  Rect anc = R;
  for (int i = 0; i < nA; i++) {
    int pd = view._model.node(A[i].parent).content.data.grid.gridDimension;
    anc = upLayout(anc, pd, view._gapRatio, A[i].row, A[i].col);
  }
  Rect rc = anc;
  for (int i = nB - 1; i >= 0; i--)
    rc = childCellLayout(view, B[i].parent, rc, B[i].row, B[i].col);
  nx = rc.cx(); ny = rc.cy();
  halfNDC = rc.halfW();
  return true;
}

int LayoutEngine::variationOf(const GameView &view, int nodeIndex) {
  if (nodeIndex < 0 || nodeIndex >= view._model.totalNodes()) return 0;
  const int p = view._model.node(nodeIndex).parentId;
  if (p >= 0 && view._model.typeOf(p) == CellType::GRID)
    return view._model.node(p).content.data.grid.variation;
  return 0;
}

bool LayoutEngine::isDescendant(const GameView &view, int ancestor, int node) {
  while (node >= 0) {
    if (node == ancestor) return true;
    node = view._model.node(node).parentId;
  }
  return false;
}
