#pragma once
class GameView;

class CoordinateMapper {
 public:
  static void screenToWorld(const GameView &view, int pixelX, int pixelY, int winW, int winH,
                            float &worldX, float &worldY);
  static void screenToNdc(const GameView &view, int pixelX, int pixelY, int winW, int winH,
                          float &nx, float &ny);
  static bool screenToGrid(const GameView &view, int pixelX, int pixelY, int winW, int winH,
                           int &row, int &col);
  static float gridToWorldX(const GameView &view, int col);
  static float gridToWorldY(const GameView &view, int row);
};
