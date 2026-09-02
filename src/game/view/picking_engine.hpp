#pragma once
#include "types.hpp"
class GameView;

class PickingEngine {
 public:
  static int resolveLeafCell(const GameView &view, float worldX, float worldY);
  static int resolveCellAt(const GameView &view, float worldX, float worldY, int nodeIndex,
                           int gridDim, Rect r);
  static int resolveCellAtWithSizeCheck(const GameView &view, float worldX, float worldY,
                                        int nodeIndex, int gridDim, Rect r);
  static int resolveCenterCell(const GameView &view, float worldX, float worldY);
};
