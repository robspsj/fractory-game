#include "print_state.hpp"
#include "game/game_model.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

void clearScreen() { std::cout << "\033[2J\033[H"; }

static int digitWidth(int n) {
  int w = 1;
  while (n >= 10) {
    w++;
    n /= 10;
  }
  return w;
}

static const char *_itemColors[GameModel::ELEMS] = {
    "\033[1;34m", "\033[1;31m", "\033[1;32m", "\033[1;33m", "\033[1;35m"};

static void printCell(std::ostream &os, int arrayIdx, const Cell &cell,
                      int idxWidth, int subgridLetterIdx) {
  int contentWidth = 3;
  switch (cell.type) {
  case CellType::ITEM:
    os << "["
       << _itemColors[cell.data.item.id] << cell.data.item.id << "\033[0m"
       << ":" << cell.data.item.count << "]";
    break;
  case CellType::GRID: {
    os << "{" << "\033[1m#\033[0m:" << subgridLetterIdx << "}";
    break;
  }
  default: {
    std::ostringstream content;
    int pad = contentWidth - (int)content.tellp();
    for (int i = 0; i < pad; i++)
      content << "_";
    os << "[" << content.str() << "]";
    break;
  }
  }
}

void printState(const GameModel &model) {
  int total = model.totalNodes();
  int idxWidth = digitWidth(total);

  int dragId = model.dragItemId();
  int dragAmount = model.dragAmount();

  std::cout << "Nodes: " << total << std::endl;

  if (dragId != -1) {
    std::cout << "Drag:  [" << _itemColors[dragId] << dragId << "\033[0m" << ":"
              << dragAmount << "]" << std::endl;
  } else {
    std::cout << "Drag:  None" << std::endl;
  }

  std::cout << std::endl << "Root:" << std::endl;
  std::cout << "{"<< "\033[1m#\033[0m:a}"
            << std::endl;

  int subgridCount = -1;


  for (int i = 0; i < total; i++) {
    const Cell &gridCell = model.node(i);
    if (gridCell.type == CellType::GRID) {
        subgridCount++;
        int subSize = gridCell.data.grid.gridDimension;
        int subFirst = gridCell.data.grid.firstChild;
        std::cout << std::endl << "Subgrid " << subgridCount << ": " << std::endl;

        int subSubCount = subgridCount;
        for (int j = 0; j < subSize; ++j) {
          for (int k = 0; k < subSize; ++k) {
            int idx = subFirst + j*subSize + k;
            const Cell &cell = model.node(idx);
            if (cell.type == CellType::GRID)
              subSubCount ++;

            printCell(std::cout, idx , cell, idxWidth, subSubCount);
            std::cout << " ";
          }
          std::cout << std::endl;
        }
        std::cout << std::endl;
    }
  }
}
