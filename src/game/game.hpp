#pragma once
#include "../gl.hpp"
#include <vector>
#include <memory>

enum class CellType {
    EMPTY,
    ITEM,
    SUBGRID
};

struct Cell {
    CellType type;
    int item_count;
    int max_capacity;
    std::shared_ptr<std::vector<std::vector<Cell>>> subgrid;

    Cell() : type(CellType::EMPTY), item_count(0), max_capacity(1) {}
};

void gameInit(unsigned int seed = 0);
void gameUpdate(int mousePx, int mousePy, int winW, int winH);
void gameMouseDown(int mousePx, int mousePy, int winW, int winH);
void gameMouseUp(int mousePx, int mousePy, int winW, int winH);
void gameRender(int winW, int winH);
GLuint gameProgram();

// Testing API - updated to return/set a more complex structure if needed
// For now, let's keep the API working for the current test runner by serializing
// the cell data into a flattened int array for the existing test runner.
void gameSetState(int grid[5][5]);
void gameGetState(int grid[5][5]);
