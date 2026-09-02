#include "grid_cell.hpp"
#include "game_model.hpp"

void GridCell::populateWithSubgrid(int cellIndex, int size, std::vector<Cell> &nodes) {
  auto &cell = nodes[cellIndex];
  cell.content.type = CellType::GRID;
  cell.content.data.grid.gridDimension = size;
  cell.content.data.grid.firstChild = (int)nodes.size();
  cell.content.data.grid.variation = size;
  for (int j = 0; j < size * size; j++) {
    Cell child;
    child.parentId = cellIndex;
    child.content.type = CellType::EMPTY;
    nodes.push_back(child);
  }
}

void GridCell::interact(int, GameModel &) {
  // No-op for now
}

void GridCell::tick(int, GameModel &) {
  // No-op for now
}
