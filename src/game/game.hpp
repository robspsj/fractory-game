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
  GameView& view() { return *_view; }
  float zoomFactor() const;
  int lastCellDrawCount() const;
  int anchorDepth() const;
  int puffSources() const;   // distinct burst sources alive in the pool
  int puffsDrawn() const;    // puffs actually drawn last frame (visible)
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
  float _dragMX = 0.0f, _dragMY = 0.0f;

  // Drain the model's cell-change events and spawn matching smoke bursts.
  void applyEffects();

  StepProfile _profile;   // timing of the last step() call (profiling)
};