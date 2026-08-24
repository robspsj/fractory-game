#pragma once
#include "config.hpp"
#include "game_model.hpp"
#include "game_view.hpp"
#include <blend2d.h>
#include <memory>

// Platform-independent key codes (mapped from SDL2 / DOM in main).
enum class Key {
  NONE = 0,
  F1,
  F2,
  UNKNOWN,
};

class Game {
public:
  Game(const Config &cfg = Config{});
  ~Game();
  void update(int mousePx, int mousePy, int winW, int winH);
  void mouseDown(int button, int mod, int mousePx, int mousePy, int winW, int winH);
  void mouseUp(int button, int mousePx, int mousePy, int winW, int winH);
  void mouseWheel(float dx, float dy, int mousePx, int mousePy, int winW, int winH);
  void keyDown(Key key, bool shift, int winW, int winH);
  void render(int winW, int winH);
  // Render into an externally-owned BLImage (e.g. SDL window surface memory).
  void renderTo(BLImage& target, int winW, int winH);
  // Advance model logic. Returns whether a model tick ran.
  bool step(int elapsedMs);
  const BLImage& image() const { return _view->image(); }
  GameView& view() { return *_view; }
  float zoomFactor() const;
  int lastCellDrawCount() const;
  int anchorDepth() const;
  float lastGenMs() const;
  float lastDrawMs() const;

private:
  enum class MouseState {
    NONE,
    HOVERING_GRID_FOR_FOCUS,
  };

  std::unique_ptr<GameModel> _model;
  std::unique_ptr<GameView> _view;

  int _ticksPerSecond;
  int _tickAccumMs = 0;

  MouseState _mouseState = MouseState::NONE;
  float _dragMX = 0.0f, _dragMY = 0.0f;
};