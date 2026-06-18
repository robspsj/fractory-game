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

  if (_mouseState != MouseState::PANNING && !_model->hasDrag()) {
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
  }

  if (_mouseState == MouseState::DOWN_PENDING) {
    int dx = mousePx - _mouseDownX;
    int dy = mousePy - _mouseDownY;
    if (std::abs(dx) > 5 || std::abs(dy) > 5) {
      Uint64 heldTime = SDL_GetTicks() - _mouseDownTime;
      if (heldTime >= 250) {
        _mouseState = MouseState::PANNING;
        _view->startPan(mousePx, mousePy);
        changedState = true;
      } else {
        int row, col;
        if (_view->screenToGrid(_mouseDownX, _mouseDownY, winW, winH, row,
                                col)) {
          int idx = _model->rootChild(row, col);
          if (_model->node(idx).type == CellType::ITEM) {
            _model->pickUp(idx, _model->node(idx).data.item.count);
            _mouseState = MouseState::DRAGGING_ITEM;
            changedState = true;
            logState();
          }
        }
      }
    }
  }

  if (_view->isPanning()) {
    _view->continuePan(mousePx, mousePy, winW, winH);
  }

  if (_mouseState == MouseState::DRAGGING_ITEM ||
      _mouseState == MouseState::DOWN_PENDING) {
    float aspect = (float)winW / (float)winH;
    _view->screenToWorld(mousePx, mousePy, winW, winH, _dragMX, _dragMY);
    _view->setDragWorldPos(_dragMX, _dragMY);
  }

  if (changedState) {
    // TODO: Update cursor based on _mouseState
  }
}

void Game::mouseDown(int button, int mousePx, int mousePy, int winW, int winH) {
  if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT) {
    int row, col;
    if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
      _mouseState = MouseState::DOWN_PENDING;
      _mouseDownX = mousePx;
      _mouseDownY = mousePy;
      _mouseDownTime = SDL_GetTicks();
    } else {
      _mouseState = MouseState::PANNING;
      _view->startPan(mousePx, mousePy);
    }
    return;
  }

  if (button == SDL_BUTTON_LEFT) {
    _mouseState = MouseState::DOWN_PENDING;
    _mouseDownX = mousePx;
    _mouseDownY = mousePy;
    _mouseDownTime = SDL_GetTicks();
  }
}

void Game::mouseUp(int button, int mousePx, int mousePy, int winW, int winH) {
  bool stateChanged = false;

  if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT) {
    if (_mouseState == MouseState::PANNING) {
      _view->endPan();
      stateChanged = true;
    } else if (_mouseState == MouseState::DOWN_PENDING) {
      int row, col;
      if (_view->screenToGrid(_mouseDownX, _mouseDownY, winW, winH, row, col)) {
        if (button == SDL_BUTTON_RIGHT) {
          if (_view->isFocused()) {
            if (!_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
              _view->unfocusGrid();
              stateChanged = true;
            }
          } else {
            const auto &node = _model->node(_model->rootChild(row, col));
            if (node.type == CellType::GRID) {
              _view->focusGrid(_model->rootChild(row, col));
              stateChanged = true;
            }
          }
        }
      } else {
        if (_view->isFocused()) {
          _view->unfocusGrid();
          stateChanged = true;
        }
      }
    }
    _mouseState = MouseState::NONE;
    stateChanged = true;
  }

  if (button == SDL_BUTTON_LEFT) {
    if (_mouseState == MouseState::DOWN_PENDING) {
      int dx = mousePx - _mouseDownX;
      int dy = mousePy - _mouseDownY;
      bool moved = std::abs(dx) > 5 || std::abs(dy) > 5;

      if (moved) {
        int dropRow, dropCol;
        if (_view->screenToGrid(mousePx, mousePy, winW, winH, dropRow,
                                dropCol)) {
          _model->drop(_model->rootChild(dropRow, dropCol));
        } else {
          _model->cancelDrag();
        }
        logState();
      } else {
        int row, col;
        if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
          int idx = _model->rootChild(row, col);
          if (_model->node(idx).type == CellType::ITEM) {
            if (!_model->hasDrag()) {
              _model->pickUp(idx, _model->node(idx).data.item.count);
              logState();
            } else {
              _model->drop(_model->rootChild(row, col));
              logState();
            }
          }
        } else {
          if (_model->hasDrag()) {
            _model->cancelDrag();
            logState();
          }
        }
      }
    } else if (_mouseState == MouseState::DRAGGING_ITEM) {
      int row, col;
      if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
        _model->drop(_model->rootChild(row, col));
      } else {
        _model->cancelDrag();
      }
      logState();
    } else if (_mouseState == MouseState::PANNING) {
      _view->endPan();
    }
    stateChanged = true;
    _mouseState = MouseState::NONE;
  }

  if (stateChanged) {
    // TODO: Update cursor/visual feedback
  }
}

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

void Game::mouseWheel(float dx, float dy) {
  if (dy > 0)
    _view->zoom(1.1f);
  else if (dy < 0)
    _view->zoom(1.0f / 1.1f);
}

void Game::render(int winW, int winH) { _view->render(winW, winH); }

GLuint Game::program() const { return _view->program(); }

void Game::setFullState(int *inData) { _model->setFullState(inData); }

void Game::getFullState(int *outData) { _model->getFullState(outData); }

void Game::printState() { ::printState(*_model); }

void Game::getDragState(int &outId, int &outCount) {
  _model->getDragState(outId, outCount);
}
