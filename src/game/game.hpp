#pragma once
#include "../gl.hpp" // IWYU pragma: keep
#include "config.hpp"
#include "game_model.hpp"
#include "game_view.hpp"
#include <SDL3/SDL.h>
#include <memory>

class Game {
public:
  Game(const Config &cfg = Config{});
  void update(int mousePx, int mousePy, int winW, int winH);
  void mouseDown(int button, int mousePx, int mousePy, int winW, int winH);
  void mouseUp(int button, int mousePx, int mousePy, int winW, int winH);
  void mouseWheel(float dx, float dy, int mousePx, int mousePy, int winW, int winH);
  void keyDown(SDL_Keycode key, SDL_Keymod mod, int winW, int winH);
  void render(int winW, int winH);
  GLuint program() const;
  float zoomFactor() const;
  int lastVertexCount() const;
  int anchorDepth() const;

  void setFullState(int *inData);
  void getFullState(int *outData);
  void getDragState(int &outId, int &outCount);

private:
  enum class MouseState {
    NONE,
    HOVERING_GRID_FOR_FOCUS,
  };

  std::unique_ptr<GameModel> _model;
  std::unique_ptr<GameView> _view;

  MouseState _mouseState = MouseState::NONE;
  float _dragMX = 0.0f, _dragMY = 0.0f;
};
