#pragma once
#include "types.hpp"
class GameView;

class PickingEngine {
 public:
  static int resolveLeafCell(const GameView &view, float worldX, float worldY);
  // Gutter-tolerant flavour: the same walk, but a point that lands in the space
  // BETWEEN cells (or just off the board) resolves to the NEAREST leaf cell
  // instead of missing, and (u,v) report where the point sits inside that cell —
  // u right-positive, v up-positive, ±0.5 being the cell's own edges. Used by the
  // gap gestures (GameView::resolveGap). Pass u/v only when the caller wants the
  // tolerant behaviour; resolveLeafCell(worldX, worldY) is the strict one.
  static int resolveLeafCell(const GameView &view, float worldX, float worldY,
                             float *u, float *v);
  static int resolveCellAt(const GameView &view, float worldX, float worldY, int nodeIndex,
                           int gridDim, Rect r, float *u = nullptr, float *v = nullptr);
  static int resolveCellAtWithSizeCheck(const GameView &view, float worldX, float worldY,
                                        int nodeIndex, int gridDim, Rect r);
  static int resolveCenterCell(const GameView &view, float worldX, float worldY);
};
