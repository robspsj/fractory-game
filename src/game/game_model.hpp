#pragma once
#include "config.hpp"
#include "interact_config.hpp"
#include <vector>

enum class CellType { EMPTY, ITEM, GRID, STATION, RESERVED };

struct ItemData {
  int id;
  int count;
};

struct GridData {
  int firstChild;
  int gridDimension;
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
  int totalNodes() const { return (int)_nodes.size(); }

private:
  void initEmptyGrid(int size);
  void initRandomGrid(const Config &cfg);
  bool tryPlaceRandomStation(int gridIndex);
  void populateWithSubgrid(int cellIndex, int size);
  int findSpillTarget(int idx);

  std::vector<Cell> _nodes;
  int _dragSrcIndex = -1;
  int _dragAmount = 0;
  int _dragItemId = -1;
  InteractConfig _interactCfg;
};