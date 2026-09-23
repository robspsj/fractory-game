#pragma once
// Isolated presentation-layer particle system. Owns ALL smoke state; the game
// model and Cell carry nothing about particles.
//
// Memory model: a FIXED pool of kMaxSources (2048) lightweight SOURCE
// DESCRIPTORS — one per burst, holding {type, itemId, element, seed, spawn
// time, cell node index, resolved config, alive}. Individual puffs are NOT
// stored: every puff is derived deterministically at render from (seed, puff
// index, elapsed time). The parent cell's on-screen position is resolved from
// its node index the SAME way cells are laid out, so puffs stay glued to their
// cell exactly as it is drawn.
//
// Sources are controlled by a LIVE COUNT and a free-slot search that starts
// rolling from (lastInsert + 1) over the array length — so spawn() is ~O(1) for
// a typical mostly-live pool. When the pool is FULL a new spawn is simply
// IGNORED (count == kMaxSources). A source that dies is only flagged `alive =
// false` (no erase / no moves) and the live count is decremented. The config
// lookup is expandable: every spawn carries an EffectKind and item id, and
// PuffConfig::cfgFor(kind, itemId) is the single extension point.

#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include <array>
#include <cstddef>
#include <functional>
#include <map>

class SkCanvas;

// Which game event produced a burst. Spawns are keyed by this so later we can
// dial per-situation looks (a stronger puff for a reaction than a drop, etc.).
enum class EffectKind {
  INTERACT_REACTION, // two items combined/consumed on shift-click
  INTERACT_SPAWN,    // a rule spawned an item into an empty cell
  QUANTUM_SPAWN,     // superheated vacuum fluctuated a particle into being
  MERGE,             // a dragged stack merged into a same-id stack on drop
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

  // Spawn ONE source (burst) for a cell at `node`. Position is NOT stored — it
  // is resolved at render via NdcResolver so the puff stays glued to the cell
  // exactly as it's drawn. baseColor tints the smoke. If the pool is FULL
  // (count == kMaxSources) the new spawn is IGNORED. Otherwise a free (dead)
  // slot is found by scanning from (lastInsert + 1) forward, rolling over the
  // array length; the live count is incremented.
  void spawn(int node, const float color[3], EffectKind kind, int itemId);
  // Advance the animation clock, then sweep the fixed pool once: any live source
  // whose lifetime has elapsed is flagged dead (alive = false) and the live
  // count is decremented — a cheap death, no removal or memory moves.
  void update(double dtSec);
  // Draw live puffs from alive sources. The resolver maps each source's cell
  // node to its current on-screen NDC centre.
  void draw(SkCanvas *canvas, int winW, int winH, const NdcResolver &resolve) const;
  void clear();
  int sourceCount() const { return (int)_count; } // LIVE bursts stored
  int lastVisible() const { return _lastVisible; } // puffs drawn last frame
  // Per-frame allocation accounting, reset at the start of a frame
  // (Game::step -> beginFrame). The pool is a fixed member array (no heap), so
  // spawn() never allocates; the only allocations are first-use gradient shaders.
  void beginFrame() { _allocsFrame = 0; }
  int allocationsLast() const { return _allocsFrame; }
  std::size_t allocatedBytes() const;

  // Tiered spawning: off-screen cells are only allowed to use half the source
  // pool, reserving the other half for cells that are actually being rendered.
  // The checker (set by Game from the view's camera) tells spawn() whether a
  // node's cell is currently on-screen. It is only consulted for spawns that
  // will actually be accepted, so the hot pool-full rejection path stays cheap.
  void setOnScreenChecker(std::function<bool(int)> fn) { _onScreen = std::move(fn); }
  int offscreenCount() const { return (int)_offscreenCount; } // live off-screen sources

private: // ---------------- source descriptor ----------------
  struct Source {
    int   node;        // parent cell node index (position resolved at render)
    int   elem;        // element id -> radial-gradient shader key
    int   seed;        // per-burst seed that drives the derived puffs
    int   itemId;      // config key (future per-item look)
    EffectKind kind;   // config key (future per-kind look)
    double startTime;  // seconds on the internal anim clock (age = now - t0)
    PuffConfig cfg;    // resolved params
    bool   alive = false; // true while the source is live; false = free slot
    bool   onScreen = false; // cell was on-screen (being rendered) at spawn time
  };

  // Number of distinct irregular silhouettes, keyed by puff seed.
  static constexpr int kBlobs = 6;
  // Hard live cap — when reached, new spawns are ignored.
  static constexpr int kMaxSources = 2048;
  // Off-screen sources are capped at half the pool, reserving the other half
  // for cells that are actually being rendered (on-screen).
  static constexpr int kMaxOffscreenSources = kMaxSources / 2;
  // Per-frame cap on puffs actually drawn, so a burst-flood degrades gracefully
  // instead of tanking the frame rate.
  static constexpr int kMaxPuffsPerFrame = 1024;

  std::array<Source, kMaxSources> _sources;  // fixed pool (member, no heap)
  size_t _count = 0;                         // LIVE source count
  size_t _offscreenCount = 0;                // live sources whose cell was off-screen
  size_t _lastInsert = 0;                    // last insert idx; free-slot search
                                             // starts at (this + 1) % kMaxSources
  std::array<SkPath, kBlobs> _blobs;         // unit-space soft blobs (~r=1)
  std::map<int, sk_sp<SkShader>> _grads;     // per-element radial gradients
  SkPaint _scratch;                          // shared draw paint
  double _now = 0.0;                         // animation clock (seconds)
  int _seedCounter = 0;                      // incremental burst seed
  mutable int _lastVisible = 0;              // puffs drawn last frame (HUD)
  int _allocsFrame = 0;                      // heap allocs this frame
  std::function<bool(int)> _onScreen;        // on-screen classifier (set by Game)

  void buildBlobs();
  sk_sp<SkShader> gradientFor(int elem, const float color[3]); // lazy, cached
};