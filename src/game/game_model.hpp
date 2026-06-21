#pragma once
#include <vector>

enum class CellType { EMPTY, ITEM, GRID };

struct ItemData {
  int id;
  int count;
};

struct GridData {
  int firstChild;
  int gridDimension;
};

struct Cell {
  CellType type;
  int parent = -1;
  union {
    ItemData item;
    GridData grid;
  } data;
};

class GameModel {
public:
  static constexpr int GRID = 5;
  static constexpr int ELEMS = 5;
  static constexpr int MAX_GRIDS = 10;

  GameModel() = default;

  void init(unsigned int seed = 0);

  const Cell &node(int index) const { return _nodes[index]; }
  Cell &node(int index) { return _nodes[index]; }

  int rootChild(int row, int col) const {
    return _nodes[0].data.grid.firstChild + row * _nodes[0].data.grid.gridDimension +
           col;
  }

  bool hasDrag() const { return _dragSrcIndex != -1; }
  int dragSrcIndex() const { return _dragSrcIndex; }
  int dragItemId() const { return _dragItemId; }
  int dragAmount() const { return _dragAmount; }

  int dragRow() const {
    if (_dragSrcIndex < 0)
      return -1;
    int offset = _dragSrcIndex - _nodes[0].data.grid.firstChild;
    return offset / _nodes[0].data.grid.gridDimension;
  }
  int dragCol() const {
    if (_dragSrcIndex < 0)
      return -1;
    int offset = _dragSrcIndex - _nodes[0].data.grid.firstChild;
    return offset % _nodes[0].data.grid.gridDimension;
  }

  void pickUp(int nodeIndex, int amount);
  void drop(int nodeIndex);
  void cancelDrag();

  void setFullState(int *inData);
  void getFullState(int *outData) const;
  int getSubgridState(int subgridSeqIndex, int *outData, int &outSize) const;
  void getDragState(int &outId, int &outCount);
  int totalNodes() const { return (int)_nodes.size(); }

private:
  void populateWithSubgrid(int cellIndex, int size);

  std::vector<Cell> _nodes;
  int _dragSrcIndex = -1;
  int _dragAmount = 0;
  int _dragItemId = -1;
};
