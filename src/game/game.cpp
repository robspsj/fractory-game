#include "game.hpp"
#include <algorithm>
#include <chrono>

// Shift modifier bit, normalized across SDL2 (KMOD_SHIFT=1) and DOM (shiftKey).
static constexpr int SHIFT_MOD = 1;
static int modShiftFlag() { return SHIFT_MOD; }

Game::Game(const Config &cfg)
    : _model(std::make_unique<GameModel>()),
      _ticksPerSecond(cfg.ticksPerSecond) {
  _model->init(cfg);
  _view = std::make_unique<GameView>(*_model, cfg);
}

Game::~Game() = default;

bool Game::step(int elapsedMs) {
  if (_tickAccumMs < 0)
    _tickAccumMs = 0;
  _tickAccumMs += elapsedMs;
  int tickMs = (_ticksPerSecond > 0) ? (1000 / _ticksPerSecond) : 1000;
  bool ticked = false;
  while (_tickAccumMs >= tickMs) {
    _tickAccumMs -= tickMs;
    _model->tick();
    ticked = true;
  }
  return ticked;
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
    CellType leafType = _model->node(leafIdx).content.type;
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
    if (_mouseState == MouseState::HOVERING_GRID_FOR_FOCUS) {
      _mouseState = MouseState::NONE;
      changedState = true;
    }
  }

  if (_model->hasDrag()) {
    _view->screenToWorld(mousePx, mousePy, winW, winH, _dragMX, _dragMY);
    _view->setDragWorldPos(_dragMX, _dragMY);
  }
}

void Game::mouseDown(int button, int mod, int mousePx, int mousePy, int winW, int winH) {
  float wx, wy;
  _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);

  if (button == 1) { // left
    if (mod & (modShiftFlag())) {
      int idx = _view->resolveLeafCell(wx, wy);
      if (idx >= 0)
        _model->interact(idx);
    } else if (_model->hasDrag()) {
      int idx = _view->resolveLeafCell(wx, wy);
      if (idx >= 0)
        _model->drop(idx);
      else
        _model->cancelDrag();
    } else {
      int idx = _view->resolveLeafCell(wx, wy);
      if (idx >= 0 && _model->node(idx).content.type == CellType::ITEM) {
        _model->pickUp(idx, _model->node(idx).content.data.item.count);
        _dragMX = wx;
        _dragMY = wy;
        _view->setDragWorldPos(wx, wy);
      }
    }
  }

  if (button == 2) { // middle
    _view->startPan(mousePx, mousePy);
  }
}

void Game::mouseUp(int button, int, int, int, int) {
  if (button == 2)
    _view->endPan();
}

void Game::keyDown(Key key, bool shift, int winW, int winH) {
  (void)shift;
  (void)winW;
  (void)winH;
  if (key == Key::F1)
    _view->resetView();
}

void Game::mouseWheel(float dx, float dy, int mousePx, int mousePy, int winW,
                     int winH) {
  (void)dx;
  if (dy == 0)
    return;
  float mouseNX = (2.0f * mousePx / (float)winW) - 1.0f;
  float mouseNY = 1.0f - (2.0f * mousePy / (float)winH);
  float factor = (dy > 0) ? 1.1f : (1.0f / 1.1f);
  _view->zoom(factor, mouseNX, mouseNY);
  _view->focusCenterCell(winW, winH);
}

void Game::render(int winW, int winH) {
  _view->render(winW, winH);
}

void Game::renderTo(BLImage& target, int winW, int winH) {
  _view->renderTo(target, winW, winH);
}

float Game::zoomFactor() const { return _view->zoomFactor(); }
int Game::lastCellDrawCount() const { return _view->lastVertexCount(); }
int Game::anchorDepth() const { return _view->anchorDepth(); }
float Game::lastGenMs() const { return _view->lastGenMs(); }
float Game::lastDrawMs() const { return _view->lastDrawMs(); }
std::size_t Game::allocatedMemory() const {
  return _model->allocatedBytes() + _view->allocatedBytes();
}