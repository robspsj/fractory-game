#pragma once
#include "../gl.hpp"
#include <vector>
#include <memory>

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
    // We use a pointer to keep the footprint constant
    // In a real scenario, you might use a custom allocator or index into a global pool
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

void gameInit(unsigned int seed = 0);
void gameUpdate(int mousePx, int mousePy, int winW, int winH);
void gameMouseDown(int mousePx, int mousePy, int winW, int winH);
void gameMouseUp(int mousePx, int mousePy, int winW, int winH);
void gameRender(int winW, int winH);
GLuint gameProgram();

// Testing API - updated to return/set a more complex structure if needed
// For now, let's keep the API working for the current test runner by serializing
// the cell data into a flattened int array for the existing test runner.
void gameSetState(int* grid);
void gameGetState(int* grid);
