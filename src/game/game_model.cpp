#include "game_model.hpp"
#include <cstdlib>
#include <ctime>
#include <algorithm>

void GameModel::init(unsigned int seed) {
    if (seed != 0) std::srand(seed);
    else std::srand((unsigned)std::time(nullptr));

    _cells.resize(GRID * GRID);

    int subgridCount = 0;
    for (int i = 0; i < GRID * GRID; i++) {
        int randVal = std::rand() % 100;
        if (randVal < 50) {
            _cells[i].type = CellType::EMPTY;
        } else if (randVal < 90 || subgridCount >= 20) {
            _cells[i].type = CellType::ITEM;
            _cells[i].data.item.id = std::rand() % ELEMS;
            _cells[i].data.item.count = std::rand() % 9 + 1;
        } else {
            _cells[i].type = CellType::SUBGRID;
            _cells[i].data.subgrid.data = nullptr;
            _cells[i].data.subgrid.size = 3;
            subgridCount++;
        }
    }

    _dragRow = _dragCol = -1;
    _dragAmount = 0;
    _dragItemId = -1;
}

const Cell& GameModel::cell(int row, int col) const {
    return _cells[row * GRID + col];
}

Cell& GameModel::cell(int row, int col) {
    return _cells[row * GRID + col];
}

void GameModel::pickUp(int row, int col, int amount) {
    int idx = row * GRID + col;
    if (row < 0 || row >= GRID || col < 0 || col >= GRID) return;
    if (_cells[idx].type != CellType::ITEM) return;

    _dragRow = row;
    _dragCol = col;
    _dragItemId = _cells[idx].data.item.id;
    _dragAmount = std::min(amount, _cells[idx].data.item.count);
    _cells[idx].data.item.count -= _dragAmount;
    if (_cells[idx].data.item.count <= 0) {
        _cells[idx].type = CellType::EMPTY;
    }
}

void GameModel::drop(int row, int col) {
    if (!hasDrag()) return;

    int srcIdx = _dragRow * GRID + _dragCol;

    if (row >= 0 && row < GRID && col >= 0 && col < GRID) {
        int dstIdx = row * GRID + col;

        if (_cells[dstIdx].type == CellType::ITEM && _cells[dstIdx].data.item.id == _dragItemId) {
            _cells[dstIdx].data.item.count += _dragAmount;
        } else if (_cells[dstIdx].type == CellType::EMPTY) {
            _cells[dstIdx].type = CellType::ITEM;
            _cells[dstIdx].data.item.id = _dragItemId;
            _cells[dstIdx].data.item.count = _dragAmount;
        } else {
            Cell temp = _cells[dstIdx];
            _cells[dstIdx].type = CellType::ITEM;
            _cells[dstIdx].data.item.id = _dragItemId;
            _cells[dstIdx].data.item.count = _dragAmount;
            _cells[srcIdx] = temp;
            _dragRow = _dragCol = -1;
            _dragAmount = 0;
            _dragItemId = -1;
            return;
        }
    } else {
        _cells[srcIdx].type = CellType::ITEM;
        _cells[srcIdx].data.item.id = _dragItemId;
        _cells[srcIdx].data.item.count = _dragAmount;
    }

    _dragRow = _dragCol = -1;
    _dragAmount = 0;
    _dragItemId = -1;
}

void GameModel::cancelDrag() {
    if (!hasDrag()) return;
    int srcIdx = _dragRow * GRID + _dragCol;
    _cells[srcIdx].type = CellType::ITEM;
    _cells[srcIdx].data.item.id = _dragItemId;
    _cells[srcIdx].data.item.count = _dragAmount;
    _dragRow = _dragCol = -1;
    _dragAmount = 0;
    _dragItemId = -1;
}

void GameModel::setFullState(int* inData) {
    for (int i = 0; i < GRID * GRID; i++) {
        int id = inData[i * 2];
        int count = inData[i * 2 + 1];
        if (id == -1) {
            _cells[i].type = CellType::EMPTY;
        } else {
            _cells[i].type = CellType::ITEM;
            _cells[i].data.item.id = id;
            _cells[i].data.item.count = count;
        }
    }
}

void GameModel::getFullState(int* outData) {
    for (int i = 0; i < GRID * GRID; i++) {
        if (_cells[i].type == CellType::ITEM) {
            outData[i * 2] = _cells[i].data.item.id;
            outData[i * 2 + 1] = _cells[i].data.item.count;
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
