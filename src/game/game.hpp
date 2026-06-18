#pragma once
#include "../gl.hpp" // IWYU pragma: keep
#include "game_model.hpp"
#include "game_view.hpp"
#include <SDL3/SDL.h>
#include <memory>

class Game {
public:
  Game(unsigned int seed = 0);
  void update(int mousePx, int mousePy, int winW, int winH);
  void mouseDown(int button, int mousePx, int mousePy, int winW, int winH);
  void mouseUp(int button, int mousePx, int mousePy, int winW, int winH);
  void mouseWheel(float dx, float dy);
  void keyDown(SDL_Keycode key, SDL_Keymod mod);
  void render(int winW, int winH);
  GLuint program() const;

  void printState();
  void setFullState(int *inData);
  void getFullState(int *outData);
  void getDragState(int &outId, int &outCount);

private:
  enum class MouseState {
    NONE,
    DOWN_PENDING,
    DRAGGING_ITEM,
    PANNING,
    HOVERING_GRID_FOR_FOCUS,
  };

  void logState();

  std::unique_ptr<GameModel> _model;
  std::unique_ptr<GameView> _view;

  MouseState _mouseState = MouseState::NONE;
  int _mouseDownX = 0, _mouseDownY = 0;
  Uint64 _mouseDownTime = 0;
  float _dragMX = 0.0f, _dragMY = 0.0f;
};
