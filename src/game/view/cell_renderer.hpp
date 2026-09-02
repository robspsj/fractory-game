#pragma once
#include "types.hpp"
class GameView;

class CellRenderer {
 public:
  static void addQuad(GameView &view, Rect r, const float color[3], float alpha = 1.0f);
  static void renderEmpty(GameView &view, Rect r, const float bgColor[3] = nullptr);
  static void renderItem(GameView &view, Rect r, int itemId, int count, float scale,
                         const float bgColor[3] = nullptr, int variation = 0, int nodeIndex = -1);
  static void renderGrid(GameView &view, int nodeIndex, Rect r, int depth, int excludeChild = -1);
  static void renderCell(GameView &view, int nodeIndex, Rect r, int depth);
  static void renderAnchor(GameView &view, int anchorIndex, Rect r, int depth, int excludeChild = -1);
};
