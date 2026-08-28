#pragma once
// Isolated presentation-layer particle system. Owns ALL smoke state; the game
// model and Cell carry nothing about particles.
//
// Memory model: the pool stores only lightweight SOURCE DESCRIPTORS — one per
// burst, holding {type, itemId, element, seed, spawn time, cell world centre,
// cell half-extent, resolved config}. Individual puffs are NOT stored: at render
// time every puff is derived deterministically from (source.seed, puff index,
// elapsed time) — position, drift, growth, rotation and fade are all computed on
// the fly. This is why the pool can hold tens of thousands of sources cheaply,
// and puffs stay stable/deterministic.
//
// The config lookup is expandable: every spawn carries an EffectKind and item
// id, and PuffConfig::cfgFor(kind, itemId) is the single extension point for
// per-situation or per-item looks (uniform default today).

#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include <array>
#include <cstddef>
#include <map>
#include <vector>
class SkCanvas;

// Which game event produced a burst. Spawns are keyed by this so later we can
// dial per-situation looks (a stronger puff for a reaction than a drop, etc.).
enum class EffectKind {
  INTERACT_REACTION, // two items combined/consumed on shift-click
  INTERACT_SPAWN,    // a rule spawned an item into an empty cell
  DROP,              // an item was dropped into a cell
  SPILL,             // tick: an item spilled to a neighbour
  DESTROY,           // tick: an item's count hit 0 / was removed
  STATION_OUTPUT,    // tick: a station placed its output
};

// Fully tunable per-burst behaviour. Sizes/scatter are fractions of the parent
// cell's half-extent, so puffs stay proportional to the cell under any zoom /
// focus. cfgFor() may return specialised variants per kind/item later.
struct PuffConfig {
  int   count    = 10;      // puffs per burst (implicit, derived at draw)
  float life     = 1.00f;   // seconds the source lives
  float size0    = 0.28f;   // start radius, fraction of cell half-size
  float size1    = 1.60f;   // end radius fraction (big, visible expansion)
  float alpha0   = 0.75f;   // start alpha (fades to 0)
  float driftUp  = 0.22f;   // world-units/sec (scaled by the cell) upward rise
  float spread   = 0.35f;   // origin scatter, fraction of cell half-size
  float swirl    = 2.5f;    // per-puff roll speed multiplier

  static PuffConfig cfgFor(EffectKind /*kind*/, int /*itemId*/);
};

class ParticleSystem {
public:
  ParticleSystem();
  ~ParticleSystem(); // defined in .cpp so sk_sp<SkShader> is complete there
  ParticleSystem(const ParticleSystem &) = delete;
  ParticleSystem &operator=(const ParticleSystem &) = delete;

  // Spawn ONE source (burst) anchored at world/scene (wx,wy) for a cell whose
  // half-extent (world) is cellHalfWorld. baseColor tints the smoke.
  void spawn(float wx, float wy, float cellHalfWorld, const float color[3],
             EffectKind kind, int itemId);
  // Advance the internal animation clock and drop expired sources.
  void update(double dtSec);
  // Draw all live sources; puffs are derived here from seed + elapsed time.
  void draw(SkCanvas *canvas, float zoom, float panX, float panY, float aspect,
            int winW, int winH) const;
  void clear();
  int sourceCount() const { return (int)_sources.size(); } // live bursts
  int lastVisible() const { return _lastVisible; }         // puffs drawn last frame
  std::size_t allocatedBytes() const;

private: // ---------------- source descriptor ----------------
  struct Source {
    float x, y;        // world centre of the parent cell
    float half;        // parent cell half-extent (world)
    int   elem;        // element id -> radial-gradient shader key
    int   seed;        // per-burst seed that drives the derived puffs
    int   itemId;      // config key (future per-item look)
    EffectKind kind;   // config key (future per-kind look)
    double startTime;  // seconds on the internal anim clock (age = now - t0)
    PuffConfig cfg;    // resolved params
  };

  // Number of distinct irregular silhouettes, keyed by puff seed.
  static constexpr int kBlobs = 6;
  // Hard cap on simultaneous live SOURCES (tiny descriptors, so this is large).
  static constexpr int kMaxSources = 32768;

  std::vector<Source> _sources;
  std::array<SkPath, kBlobs> _blobs;            // unit-space soft blobs (~r=1)
  std::map<int, sk_sp<SkShader>> _grads;        // per-element radial gradients
  SkPaint _scratch;                             // shared draw paint
  double _now = 0.0;                            // animation clock (seconds)
  int _seedCounter = 0;                         // incremental burst seed
  mutable int _lastVisible = 0;                 // puffs drawn last frame (HUD)

  void buildBlobs();
  sk_sp<SkShader> gradientFor(int elem, const float color[3]); // lazy, cached
};