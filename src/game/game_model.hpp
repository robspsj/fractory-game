#pragma once
#include <vector>

enum class CellType {
    EMPTY,
    ITEM,
    SUBGRID
};

struct ItemData {
    int id;
    int count;
};

struct SubgridData {
    void* data;
    int size;
};

struct Cell {
    CellType type;
    union {
        ItemData item;
        SubgridData subgrid;
    } data;
};

class GameModel {
public:
    static constexpr int GRID = 5;
    static constexpr int ELEMS = 5;

    GameModel() = default;

    void init(unsigned int seed = 0);

    const Cell& cell(int row, int col) const;
    Cell& cell(int row, int col);

    bool hasDrag() const { return _dragItemId != -1; }
    int dragRow() const { return _dragRow; }
    int dragCol() const { return _dragCol; }
    int dragItemId() const { return _dragItemId; }
    int dragAmount() const { return _dragAmount; }
    int dragSrcRow() const { return _dragRow; }
    int dragSrcCol() const { return _dragCol; }

    void pickUp(int row, int col, int amount);
    void drop(int row, int col);
    void cancelDrag();

    void setFullState(int* inData);
    void getFullState(int* outData);
    void getDragState(int& outId, int& outCount);

    bool isTestMode() const { return _isTestMode; }
    void setTestMode(bool v) { _isTestMode = v; }

private:
    std::vector<Cell> _cells;
    int _dragRow = -1, _dragCol = -1;
    int _dragAmount = 0;
    int _dragItemId = -1;
    bool _isTestMode = false;
};
