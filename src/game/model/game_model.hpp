#pragma once
#include "../config.hpp"
#include "../interact_config.hpp"
#include "cell.hpp"
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
  int totalNodes() const { return (int)_nodes.size(); }
  std::size_t allocatedBytes() const {
    return _nodes.capacity() * sizeof(Cell) + _adj.capacity() * sizeof(Adjacency);
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
  void emitEvent(CellEventType t, int idx, int itemId) {
    _events.push_back({idx, itemId, t});
    ++_tprof.events;
  }

  uint32_t prng(int idx, int salt) const;

  void rebuildAdjacency();

  std::vector<Cell> _nodes;
  std::vector<Adjacency> _adj;  // parallel to _nodes (see Adjacency above)
  std::vector<CellEvent> _events;
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
