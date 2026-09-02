#pragma once
class GameView;

class CameraController {
 public:
  static void zoom(GameView &view, float factor, float mouseNX, float mouseNY);
  static void startPan(GameView &view, int pixelX, int pixelY);
  static void continuePan(GameView &view, int pixelX, int pixelY, int winW, int winH);
  static void endPan(GameView &view);
};
