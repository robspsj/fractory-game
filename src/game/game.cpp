#include "game.hpp"
#include "../print_state.hpp"
#include "../shader.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

Game::Game(unsigned int seed) {
  _model = std::make_unique<GameModel>();
  _model->init(seed);
  _view = std::make_unique<GameView>(*_model);
  _view->initGL();
}

void Game::logState() {
  clearScreen();
  ::printState(*_model);
}

void Game::update(int mousePx, int mousePy, int winW, int winH) {
  bool changedState = false;

  int row, col;
  bool overGridCell =
      _view->screenToGrid(mousePx, mousePy, winW, winH, row, col);

  if (overGridCell) {
    _view->setHoveredCell(row, col);
    const auto &node = _model->node(_model->rootChild(row, col));
    if (node.type == CellType::GRID) {
      if (_mouseState != MouseState::HOVERING_GRID_FOR_FOCUS) {
        _mouseState = MouseState::HOVERING_GRID_FOR_FOCUS;
        changedState = true;
      }
    } else {
      if (_mouseState == MouseState::HOVERING_GRID_FOR_FOCUS) {
        _mouseState = MouseState::NONE;
        changedState = true;
      }
    }
  } else {
    _view->clearHoveredCell();
    if (_mouseState == MouseState::HOVERING_GRID_FOR_FOCUS) {
      _mouseState = MouseState::NONE;
      changedState = true;
    }
  }

  if (_model->hasDrag()) {
    _view->screenToWorld(mousePx, mousePy, winW, winH, _dragMX, _dragMY);
    _view->setDragWorldPos(_dragMX, _dragMY);
  }

  if (changedState) {
    // TODO: Update cursor based on _mouseState
  }
}

void Game::mouseDown(int button, int mousePx, int mousePy, int winW, int winH) {
  if (button == SDL_BUTTON_LEFT) {
    if (_model->hasDrag()) {
      int row, col;
      if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col))
        _model->drop(_model->rootChild(row, col));
      else
        _model->cancelDrag();
      logState();
    } else {
      int row, col;
      if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
        int idx = _model->rootChild(row, col);
        if (_model->node(idx).type == CellType::ITEM) {
          _model->pickUp(idx, _model->node(idx).data.item.count);
          logState();
        }
      }
    }
  }

  if (button == SDL_BUTTON_RIGHT) {
    int row, col;
    if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
      const auto &node = _model->node(_model->rootChild(row, col));
      if (node.type == CellType::GRID)
        _view->focusGrid(_model->rootChild(row, col));
    } else if (_view->isFocused()) {
      _view->unfocusGrid();
    }
  }
}

void Game::mouseUp(int, int, int, int, int) {}

void Game::keyDown(SDL_Keycode key, SDL_Keymod mod) {
  (void)mod;
#ifdef FRACTORY_DEBUG
  if (key == SDLK_EQUALS || key == SDLK_PLUS) {
    _view->focusOffset(1);
  } else if (key == SDLK_MINUS) {
    _view->focusOffset(-1);
  }
#endif
}

void Game::mouseWheel(float dx, float dy, int mousePx, int mousePy, int winW,
                     int winH) {
  if (dy == 0)
    return;
  float mouseNX = (2.0f * mousePx / (float)winW) - 1.0f;
  float mouseNY = 1.0f - (2.0f * mousePy / (float)winH);
  float factor = (dy > 0) ? 1.1f : (1.0f / 1.1f);
  _view->zoom(factor, mouseNX, mouseNY);
}

void Game::render(int winW, int winH) { _view->render(winW, winH); }

GLuint Game::program() const { return _view->program(); }

void Game::setFullState(int *inData) { _model->setFullState(inData); }

void Game::getFullState(int *outData) { _model->getFullState(outData); }

void Game::printState() { ::printState(*_model); }

void Game::getDragState(int &outId, int &outCount) {
  _model->getDragState(outId, outCount);
}
