#pragma once
class GameView;

class AnchorManager {
 public:
  static void focusTransform(GameView &view, int targetIdx);
  static void resetView(GameView &view);
  static void focusCenterCell(GameView &view, int winW, int winH);
  static bool unfocusOneLevel(GameView &view);
};
