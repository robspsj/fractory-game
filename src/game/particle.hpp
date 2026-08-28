#pragma once
// Isolated presentation-layer particle system. Owns ALL smoke state; the game
// model and Cell carry nothing about particles.
//
// Memory model: the pool stores only lightweight SOURCE DESCRIPTORS — one per
// burst, holding {type, itemId, element, seed, spawn time, cell node index,
// resolved config}. Individual puffs are NOT stored: at render time every puff
// is derived deterministically from (source.seed, puff index, elapsed time) —
// position, drift, growth, rotation and fade are all computed on the fly. The
// parent cell's on-screen position is resolved from its node index the SAME way
// cells are laid out, so puffs stay glued to their cell exactly as it is drawn.
//
// The config lookup is expandable: every spawn carries an EffectKind and item
// id, and PuffConfig::cfgFor(kind, itemId) is the single extension point for
// per-situation or per-item looks (uniform default today).

#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include <array>
#include <cstddef>
#include <functional>
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
  GROW,              // tick: an item's count increased
  SHRINK,            // tick: an item's count decreased
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
  // Resolves a cell node's on-screen NDC centre + half-extent the way the
  // renderer lays cells out (anchor-tree walk). Provided by GameView so a puff
  // positioned at a cell is always drawn exactly where that cell is drawn.
  using NdcResolver = std::function<bool(int node, float& nx, float& ny, float& halfNDC)>;

  ParticleSystem();
  ~ParticleSystem(); // defined in .cpp so sk_sp<SkShader> is complete there
  ParticleSystem(const ParticleSystem &) = delete;
  ParticleSystem &operator=(const ParticleSystem &) = delete;

  // Spawn ONE source (burst) for a cell at `node`. The node's position is NOT
  // stored — it is resolved at render time via NdcResolver so the puff stays
  // glued to the cell exactly as it's drawn. baseColor tints the smoke. Sources
  // are always stored (ring-evicting the oldest only once the pool is full);
  // display culling is deferred entirely to render().
  void spawn(int node, const float color[3], EffectKind kind, int itemId);
  // Advance the internal animation clock only. Does NOT remove sources — whether
  // a source/puff is shown (expired/offscreen/faded/tiny) is decided at render.
  void update(double dtSec);
  // Draw live puffs from stored sources; this is the ONLY culling point. The
  // resolver maps each source's cell node to its current on-screen NDC centre.
  void draw(SkCanvas *canvas, int winW, int winH, const NdcResolver &resolve) const;
  void clear();
  int sourceCount() const { return (int)_count; } // live bursts stored
  int lastVisible() const { return _lastVisible; } // puffs drawn last frame
  // Per-frame allocation accounting: reset at the start of a frame
  // (Game::step -> beginFrame), then incremented by any heap alloc the particle
  // layer does. The pool's ring storage is reserved once, so spawn() never
  // reallocates; the only allocations are first-use gradient shaders.
  void beginFrame() { _allocsFrame = 0; }
  int allocationsLast() const { return _allocsFrame; }
  std::size_t allocatedBytes() const;

private: // ---------------- source descriptor ----------------
  struct Source {
    int   node;        // parent cell node index (position resolved at render)
    int   elem;        // element id -> radial-gradient shader key
    int   seed;        // per-burst seed that drives the derived puffs
    int   itemId;      // config key (future per-item look)
    EffectKind kind;   // config key (future per-kind look)
    double startTime;  // seconds on the internal anim clock (age = now - t0)
    PuffConfig cfg;    // resolved params
  };

  // Number of distinct irregular silhouettes, keyed by puff seed.
  static constexpr int kBlobs = 6;
  // Hard cap on simultaneous stored SOURCE descriptors (tiny; ring storage).
  static constexpr int kMaxSources = 8192;
  // Per-frame cap on puffs actually drawn, so a burst-flood degrades gracefully
  // (drops the oldest, keeps the newest) instead of tanking the frame rate.
  static constexpr int kMaxPuffsPerFrame = 1024;

  std::vector<Source> _sources;                 // ring storage, reserved once
  size_t _count = 0;                            // logical number stored (0..cap)
  size_t _head = 0;                             // index of the oldest stored source
  std::array<SkPath, kBlobs> _blobs;            // unit-space soft blobs (~r=1)
  std::map<int, sk_sp<SkShader>> _grads;        // per-element radial gradients
  SkPaint _scratch;                             // shared draw paint
  double _now = 0.0;                            // animation clock (seconds)
  int _seedCounter = 0;                         // incremental burst seed
  mutable int _lastVisible = 0;                 // puffs drawn last frame (HUD)
  int _allocsFrame = 0;                         // heap allocs this frame

  void buildBlobs();
  sk_sp<SkShader> gradientFor(int elem, const float color[3]); // lazy, cached
};