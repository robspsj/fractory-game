#pragma once
class GameView;

class BackgroundRenderer {
 public:
  static void drawBackground(GameView &view, int winW, int winH, double t);
};
