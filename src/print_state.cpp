#include "print_state.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

static int digitWidth(int n) {
    int w = 1;
    while (n >= 10) { w++; n /= 10; }
    return w;
}

static const char* _itemColors[GameModel::ELEMS] = {
    "\033[1;34m", "\033[1;31m", "\033[1;32m", "\033[1;33m", "\033[1;35m"
};

static void printCell(std::ostream& os, int arrayIdx, const Cell& cell, int idxWidth, int subgridLetterIdx = -1) {
    int contentWidth = idxWidth + 4;
    switch (cell.type) {
        case CellType::ITEM:
            os << "[" << std::setw(idxWidth) << arrayIdx << ":"
               << _itemColors[cell.data.item.id] << cell.data.item.id << "\033[0m" << ":"
               << cell.data.item.count << "]";
            break;
        case CellType::GRID: {
            char letter = subgridLetterIdx >= 0 ? (char)('b' + subgridLetterIdx) : '?';
            os << "{" << std::setw(idxWidth) << arrayIdx << ":\033[1m#\033[0m:" << letter << "}";
            break;
        }
        default: {
            std::ostringstream content;
            content << std::setw(idxWidth) << arrayIdx << ":";
            int pad = contentWidth - (int)content.tellp();
            for (int i = 0; i < pad; i++) content << "_";
            os << "[" << content.str() << "]";
            break;
        }
    }
}

void printState(const GameModel& model) {
    int total = model.totalNodes();
    int idxWidth = digitWidth(total);

    int dragId = model.dragItemId();
    int dragAmount = model.dragAmount();

    std::cout << "Nodes: " << total << std::endl;

    if (dragId != -1) {
        std::cout << "Drag:  [" << _itemColors[dragId] << dragId << "\033[0m" << ":" << dragAmount << "]" << std::endl;
    } else {
        std::cout << "Drag:  None" << std::endl;
    }

    int gridSize = GameModel::GRID;

    std::cout << std::endl << "Root:" << std::endl;
    std::cout << "{" << std::setw(idxWidth) << 0 << ":\033[1m#\033[0m:a}" << std::endl;

    int maxSubgridIdx = -1;
    int subgridCount = 0;

    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            if (model.node(model.rootChild(i, j)).type == CellType::GRID) {
                subgridCount++;
            }
        }
    }
    maxSubgridIdx = subgridCount - 1;

    std::cout << std::endl << "Subgrid a:" << std::endl;
    subgridCount = 0;
    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            int idx = model.rootChild(i, j);
            const Cell& cell = model.node(idx);
            int sgIdx = (cell.type == CellType::GRID) ? subgridCount++ : -1;
            printCell(std::cout, idx, cell, idxWidth, sgIdx);
            std::cout << " ";
        }
        std::cout << std::endl;
    }

    for (int s = 0; s <= maxSubgridIdx; s++) {
        std::cout << std::endl << "Subgrid " << (char)('b' + s) << ":" << std::endl;
        std::vector<int> subData(GameModel::GRID * GameModel::GRID * 2);
        int subSize;
        int subFirst = model.getSubgridState(s, subData.data(), subSize);
        for (int i = 0; i < subSize; ++i) {
            for (int j = 0; j < subSize; ++j) {
                int cellIdx = subFirst + i * subSize + j;
                printCell(std::cout, cellIdx, model.node(cellIdx), idxWidth);
                std::cout << " ";
            }
            std::cout << std::endl;
        }
    }
}
