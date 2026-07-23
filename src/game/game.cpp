#include "game.hpp"
#include "../shader.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>

Game::Game(const Config &cfg)
    : _model(std::make_unique<GameModel>()),
      _ticksPerSecond(cfg.ticksPerSecond) {
  _model->init(cfg);
  _view = std::make_unique<GameView>(*_model, cfg);
  _view->initGL();

  _tickThread = std::thread([this]() {
    using clock = std::chrono::steady_clock;
    auto tickInterval = std::chrono::microseconds(1000000 / _ticksPerSecond);
    auto nextTick = clock::now() + tickInterval;
    while (_running.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_until(nextTick);
      nextTick += tickInterval;
      if (_running.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(_modelMutex);
        _model->tick();
      }
    }
  });
}

Game::~Game() {
  _running.store(false, std::memory_order_relaxed);
  if (_tickThread.joinable())
    _tickThread.join();
}

void Game::update(int mousePx, int mousePy, int winW, int winH) {
  if (_view->isPanning()) {
    _view->continuePan(mousePx, mousePy, winW, winH);
    _view->focusCenterCell(winW, winH);
    return;
  }

  bool changedState = false;

  float wx, wy;
  _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);
  int leafIdx = _view->resolveLeafCell(wx, wy);

  if (leafIdx >= 0) {
    int rootRow, rootCol;
    _view->screenToGrid(mousePx, mousePy, winW, winH, rootRow, rootCol);
    _view->setHoveredCell(rootRow, rootCol);
    CellType leafType;
    {
      std::lock_guard<std::mutex> lock(_modelMutex);
      leafType = _model->node(leafIdx).type;
    }
    if (leafType == CellType::GRID) {
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

  {
    std::lock_guard<std::mutex> lock(_modelMutex);
    if (_model->hasDrag()) {
      _view->screenToWorld(mousePx, mousePy, winW, winH, _dragMX, _dragMY);
      _view->setDragWorldPos(_dragMX, _dragMY);
    }
  }

  if (changedState) {
    // TODO: Update cursor based on _mouseState
  }
}

void Game::mouseDown(int button, int mousePx, int mousePy, int winW, int winH) {
  float wx, wy;
  _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);

  if (button == SDL_BUTTON_LEFT) {
    std::lock_guard<std::mutex> lock(_modelMutex);
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

  if (button == SDL_BUTTON_MIDDLE) {
    _view->startPan(mousePx, mousePy);
    return;
  }


}

void Game::mouseUp(int button, int, int, int, int) {
  if (button == SDL_BUTTON_MIDDLE)
    _view->endPan();
}

void Game::keyDown(SDL_Keycode key, SDL_Keymod mod, int winW, int winH) {
  (void)mod;
  if (key == SDLK_F1) {
    _view->resetView();
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
  _view->focusCenterCell(winW, winH);
}

void Game::render(int winW, int winH) {
  std::lock_guard<std::mutex> lock(_modelMutex);
  _view->render(winW, winH);
}

GLuint Game::program() const { return _view->program(); }
float Game::zoomFactor() const { return _view->zoomFactor(); }
int Game::lastVertexCount() const { return _view->lastVertexCount(); }
int Game::anchorDepth() const { return _view->anchorDepth(); }
float Game::lastGenMs() const { return _view->lastGenMs(); }
float Game::lastUploadMs() const { return _view->lastUploadMs(); }
float Game::lastDrawMs() const { return _view->lastDrawMs(); }

void Game::setFullState(int *inData) {
  std::lock_guard<std::mutex> lock(_modelMutex);
  _model->setFullState(inData);
}

void Game::getFullState(int *outData) {
  std::lock_guard<std::mutex> lock(_modelMutex);
  _model->getFullState(outData);
}

void Game::getDragState(int &outId, int &outCount) {
  std::lock_guard<std::mutex> lock(_modelMutex);
  _model->getDragState(outId, outCount);
}
