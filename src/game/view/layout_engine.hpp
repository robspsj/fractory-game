#pragma once
#include "types.hpp"
class GameView;

class LayoutEngine {
 public:
  static Rect childCellLayout(const GameView &view, int nodeIndex, Rect r, int row, int col);
  static Rect cellWorldCenter(const GameView &view, int targetIdx);
  static bool cellCenterNDC(const GameView &view, int nodeIndex, float &nx, float &ny, float &halfNDC);
  static int variationOf(const GameView &view, int nodeIndex);
  static bool isDescendant(const GameView &view, int ancestor, int node);
};
