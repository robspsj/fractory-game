#include "game_model.hpp"
#include <algorithm>
#include <cstdlib>
#include <ctime>

void GameModel::init(unsigned int seed) {
  if (seed != 0)
    std::srand(seed);
  else
    std::srand((unsigned)std::time(nullptr));

  _nodes.clear();

  Cell root;
  _nodes.push_back(root);

  populateWithSubgrid(0, GRID);
  int gridCount = 1;

  int i = 1;
  while (i < (int)_nodes.size()) {
    auto &cell = _nodes[i];

    int randVal = std::rand() % 100;
    if (randVal < 50) {
      cell.type = CellType::EMPTY;
    } else if (randVal < 85 || gridCount >= MAX_GRIDS) {
      cell.type = CellType::ITEM;
      cell.data.item.id = std::rand() % ELEMS;
      cell.data.item.count = std::rand() % 4 + 1;
    } else {
      cell.type = CellType::GRID;
      gridCount++;
      int subSize = std::rand() % 3 + 3;
      populateWithSubgrid(i, subSize);
    }
    i++;
  }

  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
}

void GameModel::populateWithSubgrid(int cellIndex, int size) {
  auto &cell = _nodes[cellIndex];
  cell.type = CellType::GRID;
  cell.data.grid.size = size;
  cell.data.grid.firstChild = (int)_nodes.size();

  for (int j = 0; j < size * size; j++) {
    Cell child;
    child.parent = cellIndex;
    child.type = CellType::EMPTY;
    _nodes.push_back(child);
  }
}

void GameModel::pickUp(int nodeIndex, int amount) {
  if (nodeIndex <= 0 || nodeIndex >= (int)_nodes.size())
    return;
  if (_nodes[nodeIndex].type != CellType::ITEM)
    return;

  _dragSrcIndex = nodeIndex;
  _dragItemId = _nodes[nodeIndex].data.item.id;
  _dragAmount = std::min(amount, _nodes[nodeIndex].data.item.count);
  _nodes[nodeIndex].data.item.count -= _dragAmount;
  if (_nodes[nodeIndex].data.item.count <= 0) {
    _nodes[nodeIndex].type = CellType::EMPTY;
  }
}

void GameModel::drop(int nodeIndex) {
  if (!hasDrag())
    return;

  Cell dragCell;
  dragCell.type = CellType::ITEM;
  dragCell.data.item.id = _dragItemId;
  dragCell.data.item.count = _dragAmount;

  if (nodeIndex > 0 && nodeIndex < (int)_nodes.size()) {
    if (_nodes[nodeIndex].type == CellType::ITEM &&
        _nodes[nodeIndex].data.item.id == _dragItemId) {
      _nodes[nodeIndex].data.item.count += _dragAmount;
    } else if (_nodes[nodeIndex].type == CellType::EMPTY) {
      _nodes[nodeIndex] = dragCell;
    } else {
      Cell temp = _nodes[nodeIndex];
      _nodes[nodeIndex] = dragCell;
      _nodes[_dragSrcIndex] = temp;
      _dragSrcIndex = -1;
      _dragAmount = 0;
      _dragItemId = -1;
      return;
    }
  } else {
    _nodes[_dragSrcIndex] = dragCell;
  }

  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
}

void GameModel::cancelDrag() {
  if (!hasDrag())
    return;
  Cell c;
  c.type = CellType::ITEM;
  c.data.item.id = _dragItemId;
  c.data.item.count = _dragAmount;
  _nodes[_dragSrcIndex] = c;
  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
}

void GameModel::setFullState(int *inData) {
  int first = _nodes[0].data.grid.firstChild;
  int n = _nodes[0].data.grid.size * _nodes[0].data.grid.size;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    int id = inData[i * 2];
    int count = inData[i * 2 + 1];
    if (id == -2) {
      if (_nodes[idx].type != CellType::GRID) {
        _nodes[idx].type = CellType::GRID;
        _nodes[idx].data.grid.firstChild = (int)_nodes.size();
        _nodes[idx].data.grid.size = 3;
        for (int j = 0; j < 9; j++) {
          Cell child;
          child.parent = idx;
          child.type = CellType::EMPTY;
          _nodes.push_back(child);
        }
      }
    } else if (id == -1) {
      _nodes[idx].type = CellType::EMPTY;
    } else {
      _nodes[idx].type = CellType::ITEM;
      _nodes[idx].data.item.id = id;
      _nodes[idx].data.item.count = count;
    }
  }
}

void GameModel::getFullState(int *outData) const {
  int first = _nodes[0].data.grid.firstChild;
  int n = _nodes[0].data.grid.size * _nodes[0].data.grid.size;
  int subgridIdx = 0;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    if (_nodes[idx].type == CellType::ITEM) {
      outData[i * 2] = _nodes[idx].data.item.id;
      outData[i * 2 + 1] = _nodes[idx].data.item.count;
    } else if (_nodes[idx].type == CellType::GRID) {
      outData[i * 2] = -2;
      outData[i * 2 + 1] = subgridIdx++;
    } else {
      outData[i * 2] = -1;
      outData[i * 2 + 1] = 0;
    }
  }
}

int GameModel::getSubgridState(int subgridSeqIndex, int *outData,
                               int &outSize) const {
  int first = _nodes[0].data.grid.firstChild;
  int n = _nodes[0].data.grid.size * _nodes[0].data.grid.size;
  int subgridIdx = 0;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    if (_nodes[idx].type == CellType::GRID) {
      if (subgridIdx == subgridSeqIndex) {
        int childFirst = _nodes[idx].data.grid.firstChild;
        int childSize = _nodes[idx].data.grid.size;
        outSize = childSize;
        int childCount = childSize * childSize;
        for (int ci = 0; ci < childCount; ci++) {
          int cidx = childFirst + ci;
          if (_nodes[cidx].type == CellType::ITEM) {
            outData[ci * 2] = _nodes[cidx].data.item.id;
            outData[ci * 2 + 1] = _nodes[cidx].data.item.count;
          } else if (_nodes[cidx].type == CellType::GRID) {
            outData[ci * 2] = -2;
            outData[ci * 2 + 1] = -1;
          } else {
            outData[ci * 2] = -1;
            outData[ci * 2 + 1] = 0;
          }
        }
        return childFirst;
      }
      subgridIdx++;
    }
  }
  outSize = 0;
  return 0;
}

void GameModel::getDragState(int &outId, int &outCount) {
  outId = _dragItemId;
  outCount = _dragAmount;
}
