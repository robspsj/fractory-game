#include "game.hpp"
#include "particle.hpp"
#include "heat_constants.hpp"
#include <algorithm>
#include <chrono>
#include "include/core/SkCanvas.h"

Game::Game(const Config &cfg)
    : _model(std::make_unique<GameModel>()),
      _ticksPerSecond(cfg.ticksPerSecond) {
  _model->init(cfg);
  _view = std::make_unique<GameView>(*_model, cfg);
  _particles = std::make_unique<ParticleSystem>();
  _view->setParticles(_particles.get());
  // On-screen tiering now uses the view's visible-bit cache (filled once per
  // visible cell per frame during renderTo, view state only). O(1) bit test
  // per spawn, no per-particle tree walk.
  _particles->setOnScreenChecker([this](int node){ return _view->isVisible(node); });
}

Game::~Game() = default;

void Game::refreshHover() {
  // Re-derive the hovered leaf cell from the last cursor using the CURRENT view
  // transform. Runs every frame, so the highlight tracks the cursor through
  // zooms / panning / anchor changes even when the mouse is held still (no
  // motion event fired), and it stays glued to the cell under the pointer.
  if (_mouseWinW <= 0 || _mouseWinH <= 0)
    return;
  if (_view->isPanning()) {
    _view->clearHoveredCell();
    return;
  }
  float wx, wy;
  _view->screenToWorld(_mousePX, _mousePY, _mouseWinW, _mouseWinH, wx, wy);
  int leafIdx = _view->resolveLeafCell(wx, wy);
  if (leafIdx >= 0)
    _view->setHoveredCellNode(leafIdx);
  else
    _view->clearHoveredCell();
}

bool Game::step(int elapsedMs) {
  using clk = std::chrono::steady_clock;
  _particles->beginFrame(); // start per-frame allocation accounting
  refreshHover();
  // Right button: a press that outlives the tap window (or travels past the tap
  // epsilon) is a HOLD — the heat pump. Decided per frame here, not in the
  // motion handler, because a button held perfectly still emits no motion
  // events at all (the transition would never fire and the cell would never
  // warm). It runs before the tick below, so the hold's first tick already
  // carries its heat.
  if (_rightArmed && !_book.isOpen()) {
    const float dx = float(_mousePX - _rightDownPx);
    const float dy = float(_mousePY - _rightDownPy);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        clk::now() - _rightDownTime).count();
    if (dx * dx + dy * dy > TAP_EPSILON_PX * TAP_EPSILON_PX || elapsed > TAP_MAX_MS) {
      _rightArmed = false;
      _heatingHeld = true;
    }
  }
  if (_tickAccumMs < 0)
    _tickAccumMs = 0;
  _tickAccumMs += elapsedMs;
  int tickMs = (_ticksPerSecond > 0) ? (1000 / _ticksPerSecond) : 1000;
  bool ticked = false;
  double tickSum = 0.0;
  int ticks = 0;
  // Never more than one tick per rendered frame, and never carry more than
  // one tick's worth of debt: when a tick cannot fit the frame budget (huge
  // board, single-threaded wasm tick) the simulation falls behind wall-clock
  // instead of spiralling — unbounded catch-up makes each slow frame run more
  // ticks, which makes the next frame slower, down to 1-2 fps.
  if (_tickAccumMs >= tickMs) {
    _tickAccumMs -= tickMs;
    if (_tickAccumMs > tickMs) _tickAccumMs = tickMs;  // drop unreachable catch-up
    // Right-button hold: pump heat into the hovered cell before the tick runs,
    // so a superheated EMPTY vacuum is at threshold when the decay pass rolls
    // its quantum fluctuation (GameModel::quantumFluctuation). GRID cells are
    // heat-less — addHeat no-ops for them, so hovering a container is inert.
    if (_heatingHeld) {
      int hc = _view->hoveredCellNode();
      if (hc > 0) _model->addHeat(hc, Heat::QUANTUM_HOLD_HEAT_PER_TICK);
    }
    auto s = clk::now();
    _model->tick();
    tickSum += std::chrono::duration<double, std::milli>(clk::now() - s).count();
    ticks++;
    ticked = true;
  }
  // Advance the particle/smoke layer, then spawn anything the ticks produced.
  auto s1 = clk::now();
  _particles->update((double)elapsedMs / 1000.0);
  double partsMs = std::chrono::duration<double, std::milli>(clk::now() - s1).count();
  auto s2 = clk::now();
  applyEffects();
  double effectsMs = std::chrono::duration<double, std::milli>(clk::now() - s2).count();

  _profile.tickMs = tickSum;
  _profile.partsMs = partsMs;
  _profile.effectsMs = effectsMs;
  _profile.ticks = ticks;
  _profile.ticked = ticked;
  const auto &t = _model->tickProfile();
  _profile.tick = t;
  return ticked;
}

double Game::benchModelTicks(int n) {
  using clk = std::chrono::steady_clock;
  double sum = 0.0;
  std::vector<CellEvent> tmp;
  for (int i = 0; i < n; i++) {
    auto s = clk::now();
    _model->tick();
    sum += std::chrono::duration<double, std::milli>(clk::now() - s).count();
    _model->drainEvents(tmp);   // bound the event log each tick
    tmp.clear();
  }
  // Warm-up tick (caches / first-use allocs) is excluded.
  return sum / (double)n;
}

void Game::update(int mousePx, int mousePy, int winW, int winH) {
  // Always remember the last cursor so step() can re-derive the hovered cell
  // from the CURRENT view transform (even on frames with no motion event).
  _mousePX = mousePx; _mousePY = mousePy; _mouseWinW = winW; _mouseWinH = winH;

  // The recipe book is modal: while it is open the cursor only drives its
  // scroll view (dragging the bar), never the world.
  if (_book.isOpen()) {
    _book.dragTo(mousePy, winW, winH);
    return;
  }

  // If a tap is armed, cancel it when the cursor drifts beyond epsilon.
  // This handles dragging / moving before release — not a tap.
  if (_tapArmed) {
    float dx = float(mousePx - _tapDownPx);
    float dy = float(mousePy - _tapDownPy);
    if (dx*dx + dy*dy > TAP_EPSILON_PX * TAP_EPSILON_PX) {
      _tapArmed = false;
    } else {
      // Also timeout while still held: if held longer than TAP_MAX_MS it's not a tap.
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _tapDownTime).count();
      if (elapsed > TAP_MAX_MS) {
        _tapArmed = false;
      }
    }
  }

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
    CellType leafType = _model->typeOf(leafIdx);
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
    _view->screenToNdc(mousePx, mousePy, winW, winH, _dragNX, _dragNY);
    _view->setDragScreenNdc(_dragNX, _dragNY);
  }
}

void Game::handleTap(int mousePx, int mousePy, int winW, int winH) {
  float wx, wy;
  _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);

  if (_model->hasDrag()) {
    int idx = _view->resolveLeafCell(wx, wy);
    if (idx >= 0) {
      int dropId = _model->dragItemId();
      _model->drop(idx);
      float c[3];
      _view->elementColor(dropId, c);
      _particles->spawn(idx, c, EffectKind::DROP, dropId); // node -> NDC at render
      applyEffects();
    } else {
      _model->cancelDrag();
    }
    return;
  }

  // No drag: try to pick up an ITEM under the tap.
  int idx = _view->resolveLeafCell(wx, wy);
  if (idx >= 0 && _model->typeOf(idx) == CellType::ITEM) {
    _model->pickUp(idx, _model->node(idx).content.data.item.count);
    _view->screenToNdc(mousePx, mousePy, winW, winH, _dragNX, _dragNY);
    _view->setDragScreenNdc(_dragNX, _dragNY);
  }
}

void Game::mouseDown(int button, int mod, int mousePx, int mousePy, int winW, int winH) {
  // Remember cursor for hover refresh etc.
  _mousePX = mousePx; _mousePY = mousePy; _mouseWinW = winW; _mouseWinH = winH;
  (void)mod;   // no modifier gestures: the right button carries interact

  if (_book.isOpen()) {   // modal overlay: clicks only drive the book
    _book.click(mousePx, mousePy, winW, winH);
    return;
  }

  if (button == 3) { // right -> arm a CLICK (interact); a HOLD becomes the heat pump
    _heatingHeld = false;
    _rightArmed = true;
    _rightDownPx = mousePx;
    _rightDownPy = mousePy;
    _rightDownTime = std::chrono::steady_clock::now();
    return;
  }

  if (button == 2) { // middle -> panning (immediate)
    _view->startPan(mousePx, mousePy);
    // Any pending tap is cancelled when panning starts.
    _tapArmed = false;
    return;
  }

  if (button == 1) { // left -> arm a tap (pick/drop happens on release)
    _tapArmed = true;
    _tapDownPx = mousePx;
    _tapDownPy = mousePy;
    _tapButton = button;
    _tapDownWinW = winW;
    _tapDownWinH = winH;
    _tapDownTime = std::chrono::steady_clock::now();
  }
}

void Game::mouseUp(int button, int mousePx, int mousePy, int winW, int winH) {
  _mousePX = mousePx; _mousePY = mousePy; _mouseWinW = winW; _mouseWinH = winH;

  if (_book.isOpen()) {   // modal: the release only ends a scrollbar drag
    _book.endDrag();
    // A right hold that began before the book opened must not survive it.
    _heatingHeld = false;
    _rightArmed = false;
    return;
  }

  if (button == 3) { // right -> the release ends a hold; a click is an interact
    _heatingHeld = false;
    if (_rightArmed) {
      _rightArmed = false;
      float dx = float(mousePx - _rightDownPx);
      float dy = float(mousePy - _rightDownPy);
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - _rightDownTime).count();
      if (dx*dx + dy*dy <= TAP_EPSILON_PX * TAP_EPSILON_PX && elapsed <= TAP_MAX_MS) {
        float wx, wy;
        _view->screenToWorld(mousePx, mousePy, winW, winH, wx, wy);
        const int idx = _view->resolveLeafCell(wx, wy);
        if (idx >= 0) {
          // Craft first: right-clicking a matching pattern while the crafting
          // element is held crafts it. Everything else — no tool held, or no
          // pattern there — is the plain interact (reaction, id cycle, machine
          // load, spawn on an EMPTY cell). A hold never gets this far.
          if (!_model->tryCraftWithDrag(idx)) _model->interact(idx);
          applyEffects();
        }
      }
    }
    return;
  }

  if (button == 2) {
    _view->endPan();
    return;
  }

  if (button == 1 && _tapArmed) {
    bool sameButton = (button == _tapButton);
    // distance between down and up must be within epsilon
    float dx = float(mousePx - _tapDownPx);
    float dy = float(mousePy - _tapDownPy);
    float dist2 = dx*dx + dy*dy;
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _tapDownTime).count();

    bool withinDist = dist2 <= TAP_EPSILON_PX * TAP_EPSILON_PX;
    bool withinTime = elapsedMs <= TAP_MAX_MS && elapsedMs >= 0;

    _tapArmed = false;
    if (sameButton && withinDist && withinTime) {
      handleTap(mousePx, mousePy, winW, winH);
    }
    // else: not a tap -> ignore (drag/move/cancel)
  } else {
    _tapArmed = false;
  }
}

void Game::keyDown(Key key, bool shift, int winW, int winH) {
  (void)shift;
  (void)winW;
  (void)winH;
  if (key == Key::F2) {   // recipe book toggle
    _book.toggle();
    return;
  }
  if (_book.isOpen()) {
    // Modal while open: only book navigation reaches the game.
    switch (key) {
      case Key::Escape: _book.close(); return;
      case Key::Num1: _book.setCategory(0); return;
      case Key::Num2: _book.setCategory(1); return;
      case Key::Num3: _book.setCategory(2); return;
      case Key::Num4: _book.setCategory(3); return;
      case Key::Num5: _book.setCategory(4); return;
      case Key::Left: _book.cycle(-1); return;
      case Key::Right: _book.cycle(1); return;
      case Key::Up: _book.scrollBy(1.f); return;
      case Key::Down: _book.scrollBy(-1.f); return;
      default: return;
    }
  }
  if (key == Key::F1)
    _view->resetView();
}

void Game::mouseWheel(float dx, float dy, int mousePx, int mousePy, int winW,
                     int winH) {
  (void)dx;
  if (dy == 0)
    return;
  if (_book.isOpen()) {   // scroll the book instead of zooming the board
    _book.scrollBy(dy);
    return;
  }
  float mouseNX = (2.0f * mousePx / (float)winW) - 1.0f;
  float mouseNY = 1.0f - (2.0f * mousePy / (float)winH);
  float factor = (dy > 0) ? 1.1f : (1.0f / 1.1f);
  _view->zoom(factor, mouseNX, mouseNY);
  _view->focusCenterCell(winW, winH);
}

void Game::renderTo(SkCanvas* canvas, int winW, int winH) {
  _view->renderTo(canvas, winW, winH);
}

float Game::zoomFactor() const { return _view->zoomFactor(); }
int Game::lastCellDrawCount() const { return _view->lastVertexCount(); }
int Game::anchorDepth() const { return _view->anchorDepth(); }
int Game::puffSources() const { return _particles ? _particles->sourceCount() : 0; }
int Game::puffsDrawn() const { return _particles ? _particles->lastVisible() : 0; }
int Game::offscreenSources() const { return _particles ? _particles->offscreenCount() : 0; }
int Game::particleAllocs() const { return _particles ? _particles->allocationsLast() : 0; }
float Game::lastGenMs() const { return _view->lastGenMs(); }
float Game::lastDrawMs() const { return _view->lastDrawMs(); }
std::size_t Game::allocatedMemory() const {
  return _model->allocatedBytes() + _view->allocatedBytes() +
         (_particles ? _particles->allocatedBytes() : 0);
}

void Game::applyEffects() {
  float c[3];
  // Consume the event ring in place: no copy into a side vector, no clear
  // pass (GameModel::consumeEvents just walks head -> tail).
  _model->consumeEvents([&](const CellEvent &e) {
    EffectKind kind;
    switch (e.type) {
      case CellEventType::REACTION:      kind = EffectKind::INTERACT_REACTION; break;
      case CellEventType::SPAWN:         kind = EffectKind::INTERACT_SPAWN;    break;
      case CellEventType::QUANTUM_SPAWN: kind = EffectKind::QUANTUM_SPAWN;     break;
      case CellEventType::MERGE:         kind = EffectKind::MERGE;             break;
      case CellEventType::CRAFT:         kind = EffectKind::CRAFT;             break;
      case CellEventType::SPILL:         kind = EffectKind::SPILL;             break;
      case CellEventType::DESTROY:       kind = EffectKind::DESTROY;           break;
      case CellEventType::STATION_OUTPUT:kind = EffectKind::STATION_OUTPUT;    break;
      case CellEventType::GROW:          kind = EffectKind::GROW;             break;
      case CellEventType::SHRINK:        kind = EffectKind::SHRINK;           break;
      default: return;
    }
    _view->elementColor(e.itemId, c);
    _particles->spawn(e.nodeIndex, c, kind, e.itemId); // node -> NDC at render
  });
}