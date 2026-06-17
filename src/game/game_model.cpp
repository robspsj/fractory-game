#include "game_model.hpp"
#include <cstdlib>
#include <ctime>
#include <algorithm>

void GameModel::init(unsigned int seed) {
    if (seed != 0) std::srand(seed);
    else std::srand((unsigned)std::time(nullptr));

    _nodes.clear();

    Cell root;
    root.type = CellType::GRID;
    root.data.grid.firstChild = 1;
    root.data.grid.size = GRID;
    _nodes.push_back(root);

    int subgridCount = 0;
    for (int i = 0; i < GRID * GRID; i++) {
        Cell c;
        int randVal = std::rand() % 100;
        if (randVal < 50) {
            c.type = CellType::EMPTY;
        } else if (randVal < 90 || subgridCount >= 20) {
            c.type = CellType::ITEM;
            c.data.item.id = std::rand() % ELEMS;
            c.data.item.count = std::rand() % 9 + 1;
        } else {
            c.type = CellType::GRID;
            c.data.grid.firstChild = -1;
            c.data.grid.size = 3;
            subgridCount++;
        }
        _nodes.push_back(c);
    }

    _dragSrcIndex = -1;
    _dragAmount = 0;
    _dragItemId = -1;
}

void GameModel::pickUp(int nodeIndex, int amount) {
    if (nodeIndex <= 0 || nodeIndex >= (int)_nodes.size()) return;
    if (_nodes[nodeIndex].type != CellType::ITEM) return;

    _dragSrcIndex = nodeIndex;
    _dragItemId = _nodes[nodeIndex].data.item.id;
    _dragAmount = std::min(amount, _nodes[nodeIndex].data.item.count);
    _nodes[nodeIndex].data.item.count -= _dragAmount;
    if (_nodes[nodeIndex].data.item.count <= 0) {
        _nodes[nodeIndex].type = CellType::EMPTY;
    }
}

void GameModel::drop(int nodeIndex) {
    if (!hasDrag()) return;

    if (nodeIndex > 0 && nodeIndex < (int)_nodes.size()) {
        if (_nodes[nodeIndex].type == CellType::ITEM && _nodes[nodeIndex].data.item.id == _dragItemId) {
            _nodes[nodeIndex].data.item.count += _dragAmount;
        } else if (_nodes[nodeIndex].type == CellType::EMPTY) {
            _nodes[nodeIndex].type = CellType::ITEM;
            _nodes[nodeIndex].data.item.id = _dragItemId;
            _nodes[nodeIndex].data.item.count = _dragAmount;
        } else {
            Cell temp = _nodes[nodeIndex];
            _nodes[nodeIndex].type = CellType::ITEM;
            _nodes[nodeIndex].data.item.id = _dragItemId;
            _nodes[nodeIndex].data.item.count = _dragAmount;
            _nodes[_dragSrcIndex] = temp;
            _dragSrcIndex = -1;
            _dragAmount = 0;
            _dragItemId = -1;
            return;
        }
    } else {
        _nodes[_dragSrcIndex].type = CellType::ITEM;
        _nodes[_dragSrcIndex].data.item.id = _dragItemId;
        _nodes[_dragSrcIndex].data.item.count = _dragAmount;
    }

    _dragSrcIndex = -1;
    _dragAmount = 0;
    _dragItemId = -1;
}

void GameModel::cancelDrag() {
    if (!hasDrag()) return;
    _nodes[_dragSrcIndex].type = CellType::ITEM;
    _nodes[_dragSrcIndex].data.item.id = _dragItemId;
    _nodes[_dragSrcIndex].data.item.count = _dragAmount;
    _dragSrcIndex = -1;
    _dragAmount = 0;
    _dragItemId = -1;
}

void GameModel::setFullState(int* inData) {
    int first = _nodes[0].data.grid.firstChild;
    int n = _nodes[0].data.grid.size * _nodes[0].data.grid.size;
    for (int i = 0; i < n; i++) {
        int idx = first + i;
        int id = inData[i * 2];
        int count = inData[i * 2 + 1];
        if (id == -1) {
            _nodes[idx].type = CellType::EMPTY;
        } else {
            _nodes[idx].type = CellType::ITEM;
            _nodes[idx].data.item.id = id;
            _nodes[idx].data.item.count = count;
        }
    }
}

void GameModel::getFullState(int* outData) const {
    int first = _nodes[0].data.grid.firstChild;
    int n = _nodes[0].data.grid.size * _nodes[0].data.grid.size;
    for (int i = 0; i < n; i++) {
        int idx = first + i;
        if (_nodes[idx].type == CellType::ITEM) {
            outData[i * 2] = _nodes[idx].data.item.id;
            outData[i * 2 + 1] = _nodes[idx].data.item.count;
        } else {
            outData[i * 2] = -1;
            outData[i * 2 + 1] = 0;
        }
    }
}

void GameModel::getDragState(int& outId, int& outCount) {
    outId = _dragItemId;
    outCount = _dragAmount;
}
