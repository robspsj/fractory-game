#pragma once
#include "config.hpp"
#include "game_model.hpp"
#include "game_view.hpp"
#include <memory>
class SkCanvas;
class ParticleSystem;

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
  // Render into the given canvas (which wraps either the SDL window surface on
  // native or a CPU RGBA buffer on web). The canvas matrix is left set to
  // NDC->pixel; callers reset it before drawing overlays in pixel space.
  void renderTo(SkCanvas* canvas, int winW, int winH);
  // Advance model logic. Returns whether a model tick ran.
  bool step(int elapsedMs);
  GameModel& model() { return *_model; }
  GameView& view() { return *_view; }
  float zoomFactor() const;
  int lastCellDrawCount() const;
  int anchorDepth() const;
  int puffSources() const;   // distinct burst sources alive in the pool
  int puffsDrawn() const;    // puffs actually drawn last frame (visible)
  int offscreenSources() const; // live sources whose cell was off-screen at spawn
  int particleAllocs() const; // heap allocations in the particle layer, last frame
  float lastGenMs() const;
  float lastDrawMs() const;
  std::size_t allocatedMemory() const;

  // Per-stage timing of the model update (last step call), for profiling the
  // gap between the render cost and the observed frame rate.
  struct StepProfile {
    double tickMs = 0.0;     // GameModel::tick (may run several times per step)
    double partsMs = 0.0;    // ParticleSystem::update
    double effectsMs = 0.0;  // applyEffects (drain events -> spawn smoke)
    int ticks = 0;           // number of model ticks executed last step
    bool ticked = false;
    GameModel::TickProfile tick; // cost breakdown of the last model tick
  };
  StepProfile stepProfile() const { return _profile; }

  // Headless micro-benchmark: run `n` model ticks back-to-back, draining the
  // event log each tick to bound memory, and return the average tick time (ms).
  // Used to measure the sweep before/after parallelization.
  double benchModelTicks(int n);
  int modelNodeCount() const { return _model ? _model->totalNodes() : 0; }

private:
  enum class MouseState {
    NONE,
    HOVERING_GRID_FOR_FOCUS,
  };

  std::unique_ptr<GameModel> _model;
  std::unique_ptr<GameView> _view;
  std::unique_ptr<ParticleSystem> _particles;

  int _ticksPerSecond;
  int _tickAccumMs = 0;

  MouseState _mouseState = MouseState::NONE;
  float _dragNX = 0.0f, _dragNY = 0.0f;
  // Last known cursor (pixel) + window size, so the hovered cell can be
  // recomputed every frame from the CURRENT view transform — it then tracks
  // the cursor through zooms/pan/anchor changes even when the mouse is held
  // still (no motion event) and stays glued to the cell under it.
  int _mousePX = 0, _mousePY = 0, _mouseWinW = 0, _mouseWinH = 0;

  // --- Tap picking (short press+release without moving > epsilon) ---------
  static constexpr int TAP_MAX_MS = 250;      // max press duration for a tap
  static constexpr float TAP_EPSILON_PX = 8.0f; // max movement (px) during tap
  bool _tapArmed = false;
  int _tapDownPx = 0, _tapDownPy = 0;
  int _tapButton = 0;
  int _tapMod = 0;
  int _tapDownWinW = 0, _tapDownWinH = 0;
  std::chrono::steady_clock::time_point _tapDownTime{};

  // --- Right button: craft with a held tool, else heat the hovered cell ----
  // Holding (dragging) the crafting element turns the right button into a craft
  // attempt in the clicked area; with nothing held it is the heat pump that
  // drives quantum fluctuations (Heat::QUANTUM_*). The heat itself lands in
  // Game::step so the gain is tick-paced; the button state only arms it.
  bool _heatingHeld = false;

  void handleTap(int mousePx, int mousePy, int winW, int winH, int mod);

  // Drain the model's cell-change events and spawn matching smoke bursts.
  void applyEffects();
  // Recompute the hovered leaf cell from the last cursor position.
  void refreshHover();

  StepProfile _profile;   // timing of the last step() call (profiling)
};