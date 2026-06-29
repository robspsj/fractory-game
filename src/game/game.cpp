#include "game.hpp"
#include "../shader.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

Game::Game(const Config &cfg) {
  _model = std::make_unique<GameModel>();
  _model->init(cfg);
  _view = std::make_unique<GameView>(*_model);
  _view->initGL();
}

void Game::update(int mousePx, int mousePy, int winW, int winH) {
  bool changedState = false;

  float wx, wy;
  _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);
  int leafIdx = _view->resolveLeafCell(wx, wy);

  if (leafIdx >= 0) {
    int rootRow, rootCol;
    _view->screenToGrid(mousePx, mousePy, winW, winH, rootRow, rootCol);
    _view->setHoveredCell(rootRow, rootCol);
    if (_model->node(leafIdx).type == CellType::GRID) {
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
  float wx, wy;
  _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);

  if (button == SDL_BUTTON_LEFT) {
    if (_model->hasDrag()) {
      int idx = _view->resolveLeafCell(wx, wy);
      if (idx >= 0)
        _model->drop(idx);
      else
        _model->cancelDrag();
    } else {
      int idx = _view->resolveLeafCell(wx, wy);
      if (idx >= 0 && _model->node(idx).type == CellType::ITEM) {
        _model->pickUp(idx, _model->node(idx).data.item.count);
        _dragMX = wx;
        _dragMY = wy;
        _view->setDragWorldPos(wx, wy);
      }
    }
  }

  if (button == SDL_BUTTON_RIGHT) {
    int row, col;
    if (_view->screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
      int childIndex = _model->node(_view->anchorIndex()).data.grid.firstChild +
                       row * _view->anchorSize() + col;
      if (_model->node(childIndex).type == CellType::GRID)
        _view->focusGrid(childIndex);
    } else if (_view->isFocused()) {
      _view->unfocusGrid();
    }
  }
}

void Game::mouseUp(int, int, int, int, int) {}

void Game::keyDown(SDL_Keycode key, SDL_Keymod mod, int winW, int winH) {
  (void)mod;
  if (key == SDLK_P) {
    _view->focusOffset(1);
  } else if (key == SDLK_O) {
    _view->focusOffset(-1);
  } else if (key == SDLK_SPACE) {
    _view->focusCenterCell(winW, winH);
  }
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
float Game::zoomFactor() const { return _view->zoomFactor(); }

void Game::setFullState(int *inData) { _model->setFullState(inData); }

void Game::getFullState(int *outData) { _model->getFullState(outData); }

void Game::getDragState(int &outId, int &outCount) {
  _model->getDragState(outId, outCount);
}
