#include "grid_cell.hpp"
#include "game_model.hpp"

void GridCell::populateWithSubgrid(int cellIndex, int size, std::vector<Cell> &nodes,
                                   std::vector<uint8_t> &types) {
  auto &cell = nodes[cellIndex];
  types[cellIndex] = (uint8_t)CellType::GRID;
  cell.content.data.grid.gridDimension = size;
  cell.content.data.grid.firstChild = (int)nodes.size();
  cell.content.data.grid.variation = size;
  for (int j = 0; j < size * size; j++) {
    Cell child;
    child.parentId = cellIndex;
    nodes.push_back(child);
    types.push_back((uint8_t)CellType::EMPTY);
  }
}

void GridCell::interact(int, GameModel &) {
  // No-op for now
}

void GridCell::tick(int, GameModel &) {
  // No-op for now
}
