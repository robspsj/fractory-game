#pragma once
#include <cstdint>
class GameView;
struct Rect;

class IconRenderer {
 public:
  static void buildIcon(GameView &view, int icon, const float color[3]);
  static void drawIcon(GameView &view, float cx, float cy, int icon, const float color[3],
                       float halfW, float halfH, int instance = 0);
  static void renderCellItems(GameView &view, float centerX, float centerY, int count,
                              const float color[3], float scale, int icon);
  static uint32_t rgba(const float color[3], float alpha);
};
