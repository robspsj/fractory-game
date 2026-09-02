#include "camera_controller.hpp"
#include "game_view.hpp"

void CameraController::zoom(GameView &view, float factor, float mouseNX, float mouseNY) {
  float oldZoom = view._zoom;
  view._zoom *= factor;
  if (view._zoom < MIN_ZOOM) view._zoom = MIN_ZOOM;
  if (view._zoom > MAX_ZOOM) view._zoom = MAX_ZOOM;
  float actualFactor = view._zoom / oldZoom;
  view._panX = mouseNX * (1.0f - actualFactor) + view._panX * actualFactor;
  view._panY = mouseNY * (1.0f - actualFactor) + view._panY * actualFactor;
}

void CameraController::startPan(GameView &view, int pixelX, int pixelY) {
  view._isPanning = true;
  view._lastPanX = pixelX;
  view._lastPanY = pixelY;
}

void CameraController::continuePan(GameView &view, int pixelX, int pixelY, int winW, int winH) {
  float deltaX = 2.0f * (pixelX - view._lastPanX) / (float)winW;
  float deltaY = -2.0f * (pixelY - view._lastPanY) / (float)winH;
  view._panX += deltaX;
  view._panY += deltaY;
  view._lastPanX = pixelX;
  view._lastPanY = pixelY;
}

void CameraController::endPan(GameView &view) {
  view._isPanning = false;
}
