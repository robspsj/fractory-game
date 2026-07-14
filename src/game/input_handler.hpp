#pragma once
#include <SDL3/SDL.h>

class Game;

class InputHandler {
public:
  InputHandler(Game &game);

  void processEvent(const SDL_Event &e, int winW, int winH);

private:
  // Mouse passthrough
  void handleMouseMotion(const SDL_Event &e, int winW, int winH);
  void handleMouseButton(const SDL_Event &e, int winW, int winH);
  void handleMouseWheel(const SDL_Event &e, int winW, int winH);
  void handleKey(const SDL_Event &e, int winW, int winH);

  // Touch finger events
  void handleFingerDown(const SDL_Event &e, int winW, int winH);
  void handleFingerMotion(const SDL_Event &e, int winW, int winH);
  void handleFingerUp(const SDL_Event &e, int winW, int winH);

  // Pinch gesture events
  void handlePinchBegin(const SDL_Event &e, int winW, int winH);
  void handlePinchUpdate(const SDL_Event &e, int winW, int winH);
  void handlePinchEnd(const SDL_Event &e, int winW, int winH);

  // Touch helpers
  int findFinger(SDL_FingerID id) const;
  int addFinger(SDL_FingerID id, float x, float y, Uint64 timestamp);
  void removeFinger(SDL_FingerID id);
  int activeFingerCount() const;
  void getFingerCenter(float &cx, float &cy) const;
  void getFingerCenterPixels(int &cx, int &cy, int winW, int winH) const;

  void startOneFingerPan(int pixelX, int pixelY);
  void endOneFingerPan();
  void emitTap(int pixelX, int pixelY, int winW, int winH);

  Game &_game;

  struct Finger {
    SDL_FingerID id = 0;
    float startX = 0.0f, startY = 0.0f;
    float currentX = 0.0f, currentY = 0.0f;
    Uint64 startTime = 0;
    bool active = false;
  };

  static constexpr int MAX_FINGERS = 5;
  Finger _fingers[MAX_FINGERS];

  bool _touchPanning = false;
  int _lastPanPixelX = 0, _lastPanPixelY = 0;

  bool _pinchActive = false;

  static constexpr float TAP_MAX_DISTANCE = 0.02f;
  static constexpr Uint64 TAP_MAX_DURATION_NS = 200000000;
};
