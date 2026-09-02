#include "coordinate_mapper.hpp"
#include "game_view.hpp"

void CoordinateMapper::screenToWorld(const GameView &view, int pixelX, int pixelY, int winW, int winH,
                                     float &worldX, float &worldY) {
  float aspect = (float)winW / (float)winH;
  worldX = ((2.0f * pixelX / (float)winW) - 1.0f - view._panX) / view._zoom;
  worldY = ((1.0f - (2.0f * pixelY / (float)winH)) - view._panY) / aspect / view._zoom;
}

void CoordinateMapper::screenToNdc(const GameView &view, int pixelX, int pixelY, int winW, int winH,
                                   float &nx, float &ny) {
  (void)view;
  nx = 2.0f * (float)pixelX / (float)winW - 1.0f;
  ny = 1.0f - 2.0f * (float)pixelY / (float)winH;
}

bool CoordinateMapper::screenToGrid(const GameView &view, int pixelX, int pixelY, int winW, int winH,
                                    int &row, int &col) {
  float worldX, worldY;
  screenToWorld(view, pixelX, pixelY, winW, winH, worldX, worldY);

  int size = view._anchorSize;

  float childCellSize = view._anchorWidth / (size + (size - 1) * view._gapRatio);
  float pitch = childCellSize * (1 + view._gapRatio);
  float totalSize = size * pitch;
  float halfContent = childCellSize * 0.5f;

  float startX = -totalSize * 0.5f + pitch * 0.5f;
  float startY = totalSize * 0.5f - pitch * 0.5f;

  col = (int)((worldX - startX) / pitch + 0.5f);
  row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= size || col < 0 || col >= size)
    return false;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > halfContent ||
      std::abs(worldY - centerY) > halfContent)
    return false;

  return true;
}

float CoordinateMapper::gridToWorldX(const GameView &view, int col) {
  float childCellSize = view._anchorWidth / (view._anchorSize + (view._anchorSize - 1) * view._gapRatio);
  float pitch = childCellSize * (1 + view._gapRatio);
  float totalSize = view._anchorSize * pitch;
  return -totalSize * 0.5f + pitch * 0.5f + col * pitch;
}

float CoordinateMapper::gridToWorldY(const GameView &view, int row) {
  float childCellSize = view._anchorWidth / (view._anchorSize + (view._anchorSize - 1) * view._gapRatio);
  float pitch = childCellSize * (1 + view._gapRatio);
  float totalSize = view._anchorSize * pitch;
  return totalSize * 0.5f - pitch * 0.5f - row * pitch;
}
