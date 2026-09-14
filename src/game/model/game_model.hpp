#pragma once
#include "../config.hpp"
#include "../interact_config.hpp"
#include "cell.hpp"
#include "tick_plan.hpp"
#include <unordered_set>
#include <vector>

// Spatial adjacency directions (order matters: up,down,left,right — matches
// the legacy neighbor-iteration order in ItemCell).
enum AdjDir : int { ADJ_UP = 0, ADJ_DOWN = 1, ADJ_LEFT = 2, ADJ_RIGHT = 3 };

// Precomputed world-space neighbours, parallel to the node array. Each entry
// is the DEEPEST cell whose rectangle shares the midpoint of that edge (may
// cross into/out of a nested subgrid), or -1 at the world border. Entries
// always point at leaf cells (never GRID containers). GRID nodes themselves
// have no entries. Rebuilt whenever the topology changes — see
// GameModel::rebuildAdjacency.
struct Adjacency {
  int32_t n[4] = {-1, -1, -1, -1};
  int32_t &operator[](int dir) { return n[dir]; }
  int32_t operator[](int dir) const { return n[dir]; }
};

// Forward declarations for per-type behavior classes (friend access).
class EmptyCell;
class ItemCell;
class GridCell;
class StationCell;
class ReservedCell;

class GameModel {
 public:
  static constexpr int GRID = 5;
  static constexpr int ELEMS = 5;

  GameModel() = default;

  void init(const Config &cfg);

  const Cell &node(int index) const { return _nodes[index]; }
  Cell &node(int index) { return _nodes[index]; }
  CellType typeOf(int index) const { return (CellType)_type[index]; }

  int rootChild(int row, int col) const {
    return _nodes[0].content.data.grid.firstChild + row * _nodes[0].content.data.grid.gridDimension +
           col;
  }

  bool hasDrag() const { return _dragSrcIndex != -1; }
  int dragSrcIndex() const { return _dragSrcIndex; }
  int dragItemId() const { return _dragItemId; }
  int dragAmount() const { return _dragAmount; }
  uint8_t dragHeat() const { return _dragHeat; }

  int dragRow() const {
    if (_dragSrcIndex < 0)
      return -1;
    int offset = _dragSrcIndex - _nodes[0].content.data.grid.firstChild;
    return offset / _nodes[0].content.data.grid.gridDimension;
  }
  int dragCol() const {
    if (_dragSrcIndex < 0)
      return -1;
    int offset = _dragSrcIndex - _nodes[0].content.data.grid.firstChild;
    return offset % _nodes[0].content.data.grid.gridDimension;
  }

  void pickUp(int nodeIndex, int amount);
  void drop(int nodeIndex);
  void cancelDrag();
  void interact(int nodeIndex);
  bool placeStation(int gridIndex, int recipeId);
  void removeStation(int anchorIndex);

  void tick();
  void tickCell(int idx);
  void tickEmpty(int idx);
  void tickItem(int idx);
  void tickStation(int idx);
  void tickGrid(int idx);
  void tryStartStation(int anchorIdx);

  void setFullState(int *inData);
  void getFullState(int *outData) const;
  int getSubgridState(int subgridSeqIndex, int *outData, int &outSize) const;
  void getDragState(int &outId, int &outCount);
  void drainEvents(std::vector<CellEvent> &out);
  bool hasEvents() const { return _evHead != _evTail; }
  // Consume every pending event in place (no copies, no clear pass).
  template <typename F> void consumeEvents(F &&f) {
    for (; _evHead != _evTail; ++_evHead)
      f(_events[_evHead & EVENT_MASK]);
  }
  uint32_t eventDrops() const { return _eventDrops; }
  int totalNodes() const { return (int)_nodes.size(); }
  std::size_t allocatedBytes() const {
    return _nodes.capacity() * sizeof(Cell) + _adj.capacity() * sizeof(Adjacency) +
           _type.capacity();
  }

  // World-space neighbour of a leaf cell in direction d (ADJ_*), or -1.
  int neighbor(int idx, int dir) const { return _adj[idx][dir]; }

  // Heat API — only non-GRID cells carry meaningful heat (uint8_t 0..255)
  uint8_t heatAt(int idx) const;
  void setHeat(int idx, uint8_t h);
  void addHeat(int idx, int delta); // clamped 0..255, no-op for GRID
  void setHeatBulk(const std::vector<std::pair<int,uint8_t>> &pairs);

  struct TickProfile {
    double sweepMs = 0.0;
    double planMs = 0.0;     // phase-1 linear node scan (item/station planning)
    double applyMs = 0.0;    // phase-2 mutations + spill first-wins claims
    double stationMs = 0.0;  // phase-2 tickStation() loop
    double decayMs = 0.0;    // heat decay full scan (periodic)
    double diffuseMs = 0.0;  // heat diffusion full scan (periodic)
    double spillMs = 0.0;
    double emitMs = 0.0;
    int nodes = 0;
    int items = 0;
    int events = 0;
    int spills = 0;
  };
  TickProfile tickProfile() const { return _tprof; }
  TickProfile tickAccum() const { return _taccum; }  // sums over all ticks

  void setRngOverride(int v) { _rngOverride = v; }

 private:
  friend class EmptyCell;
  friend class ItemCell;
  friend class GridCell;
  friend class StationCell;
  friend class ReservedCell;

  void initEmptyGrid(int size);
  void initRandomGrid(const Config &cfg);
  bool tryPlaceRandomStation(int gridIndex);
  void populateWithSubgrid(int cellIndex, int size);
  int findSpillTarget(int idx);
  // Ring buffer of cell events: monotonic head/tail cursors, power-of-two
  // capacity (mask wrap), buffer allocated once in init(). emit/drain do zero
  // allocation AND no clear()/copy pass over the list — consumeEvents() reads
  // slots in place between the cursors. If the ring fills within one frame
  // (only possible in extreme spikes) new events drop rather than growing.
  static constexpr uint32_t EVENT_CAP  = 1u << 18;          // 262144 (~3 MB)
  static constexpr uint32_t EVENT_MASK = EVENT_CAP - 1;
  void emitEvent(CellEventType t, int idx, int itemId) {
    if (_evTail - _evHead < EVENT_CAP) {
      _events[_evTail++ & EVENT_MASK] = CellEvent{idx, itemId, t};
      ++_tprof.events;
    } else {
      ++_eventDrops;
    }
  }

  uint32_t prng(int idx, int salt) const;

  // Single choke-point for node type/content changes: keeps the compact
  // parallel _type map authoritative (the tick plan phase prescans it without
  // touching Cell cache lines). Never write _type or content directly here.
  void setType(int idx, CellType t) { _type[idx] = (uint8_t)t; }
  void putContent(int idx, CellType t, const CellContent &c) {
    _nodes[idx].content = c;
    _type[idx] = (uint8_t)t;
  }

  void rebuildAdjacency();

   std::vector<Cell> _nodes;
  std::vector<uint8_t> _type;  // parallel to _nodes: (uint8_t)CellType
  std::vector<Adjacency> _adj;  // parallel to _nodes (see Adjacency above)

  // Per-tick scratch, persistent so steady-state tick does no allocation
  // (thread-local plan buffers + the spill first-wins claim set).
  struct ThreadBuf {
    std::vector<PlMut> muts;
    std::vector<PlSpill> spills;
    std::vector<int> stations;
    long items = 0;
    double spillMs = 0.0;
  };
  std::vector<ThreadBuf> _tickBufs;
  std::unordered_set<int> _spillClaimed;
  std::vector<CellEvent> _events;  // ring storage, fixed size EVENT_CAP after init()
  uint32_t _evHead = 0;            // monotonic read cursor
  uint32_t _evTail = 0;            // monotonic write cursor
  uint32_t _eventDrops = 0;
  int _dragSrcIndex = -1;
  int _dragAmount = 0;
  int _dragItemId = -1;
  uint8_t _dragHeat = 0;
  InteractConfig _interactCfg;
  TickProfile _tprof;
  TickProfile _taccum{};
  unsigned _prngSeed = 0;
  int _tickCounter = 0;
  int _rngOverride = -1;
  int _numItems = 5;
};
