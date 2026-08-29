#pragma once
#include "config.hpp"
#include "interact_config.hpp"
#include <vector>

enum class CellType { EMPTY, ITEM, GRID, STATION, RESERVED };

// Presentational event log for the particle/effect layer. The model never
// stores particles; it only records "this cell changed in this way" so a
// subscriber (Game -> ParticleSystem) can react. Kept out of Cell entirely.
enum class CellEventType { REACTION, SPAWN, SPILL, DESTROY, STATION_OUTPUT, GROW, SHRINK };
struct CellEvent {
  int nodeIndex;
  int itemId; // element involved (for per-item effect config)
  CellEventType type;
};

struct ItemData {
  int id;
  int count;
};

struct GridData {
  int firstChild;
  int gridDimension;
  int variation;   // visual theme, derived from the grid's dimension at creation
};

struct StationData {
  int recipeIndex;
  int progress;     // ticks remaining, 0 = idle
  int sizeR, sizeC; // dimensions
};

struct ReservedData {
  int anchorIndex;
  ReserveRole role;
  int itemId;
  int itemCount;
};

struct CellContent {
  CellType type = CellType::EMPTY;
  int byteSize = 0;
  union {
    ItemData item;
    GridData grid;
    StationData station;
    ReservedData reserved;
  } data;

  CellContent() : type(CellType::EMPTY), byteSize(0) {
    data.item = {-1, 0};
  }
};

struct Cell {
  int parentId = -1;
  CellContent content;
};

inline int calcByteSize(CellType type) {
  switch (type) {
    case CellType::EMPTY:    return 9;
    case CellType::ITEM:     return 13;
    case CellType::GRID:     return 13;
    case CellType::STATION:  return 17;
    case CellType::RESERVED: return 18;
  }
  return 0;
}

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
  // Drain and clear the pending effect events (Reaction/Spill/Destroy/etc.)
  // produced by the last tick()/interact(). Game feeds these to the particles.
  void drainEvents(std::vector<CellEvent> &out);
  int totalNodes() const { return (int)_nodes.size(); }
  // Total heap owned by the model: the grid node vector (capacity, so reserved
  // but unfilled capacity is counted too). This is the dominant allocation and
  // grows with subgrid depth.
  std::size_t allocatedBytes() const { return _nodes.capacity() * sizeof(Cell); }

  // Cost breakdown of the last GameModel::tick() — the sweep over the node
  // array (which dominates the frame on large auto-boards).
  struct TickProfile {
    double sweepMs = 0.0;  // time iterating every node + per-item rand() checks
    double spillMs = 0.0;  // time inside findSpillTarget calls
    double emitMs  = 0.0;  // time appending effect events
    int nodes = 0;         // _nodes.size() swept
    int items = 0;         // ITEM cells encountered
    int events = 0;        // effect events appended
    int spills = 0;        // findSpillTarget calls
  };
  TickProfile tickProfile() const { return _tprof; }

private:
  void initEmptyGrid(int size);
  void initRandomGrid(const Config &cfg);
  bool tryPlaceRandomStation(int gridIndex);
  void populateWithSubgrid(int cellIndex, int size);
  int findSpillTarget(int idx);
  void emitEvent(CellEventType t, int idx, int itemId) {
    _events.push_back({idx, itemId, t});
    ++_tprof.events;   // profiling counter
  }

  std::vector<Cell> _nodes;
  std::vector<CellEvent> _events; // effect log drained by the Game presenter
  int _dragSrcIndex = -1;
  int _dragAmount = 0;
  int _dragItemId = -1;
  InteractConfig _interactCfg;
  TickProfile _tprof;   // profiling of the last tick()
};