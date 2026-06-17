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

static void printCell(std::ostream& os, int arrayIdx, int id, int count, int subgridIdx, int idxWidth) {
    int contentWidth = idxWidth + 4;
    if (id >= 0) {
        os << "[" << std::setw(idxWidth) << arrayIdx << ":"
           << _itemColors[id] << id << "\033[0m" << ":" << count << "]";
    } else if (id == -1) {
        std::ostringstream content;
        content << std::setw(idxWidth) << arrayIdx << ":";
        int pad = contentWidth - (int)content.tellp();
        for (int i = 0; i < pad; i++) content << "_";
        os << "[" << content.str() << "]";
    } else {
        os << "{" << std::setw(idxWidth) << arrayIdx << ":\033[1m#\033[0m:" << (char)('b' + subgridIdx) << "}";
    }
}

void printState(const GameModel& model) {
    int data[GameModel::GRID * GameModel::GRID * 2];
    model.getFullState(data);

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

    int maxSubgridIdx = -1;
    int first = 1;

    std::cout << std::endl << "Subgrid a:" << std::endl;
    for (int i = 0; i < GameModel::GRID; ++i) {
        for (int j = 0; j < GameModel::GRID; ++j) {
            int idx = first + i * GameModel::GRID + j;
            int id = data[(i * GameModel::GRID + j) * 2];
            int count = data[(i * GameModel::GRID + j) * 2 + 1];
            if (id == -2) {
                if (count > maxSubgridIdx) maxSubgridIdx = count;
                printCell(std::cout, idx, id, count, count, idxWidth);
            } else {
                printCell(std::cout, idx, id, count, -1, idxWidth);
            }
            std::cout << " ";
        }
        std::cout << std::endl;
    }

    for (int s = 0; s <= maxSubgridIdx; s++) {
        char letter = 'a' + s;
        std::cout << std::endl << "Subgrid " << (char)(letter + 1) << ":" << std::endl;
        int subData[9 * 2];
        int subSize;
        int subFirst = model.getSubgridState(s, subData, subSize);
        for (int i = 0; i < subSize; ++i) {
            for (int j = 0; j < subSize; j++) {
                int cellIdx = subFirst + i * subSize + j;
                int dataIdx = (i * subSize + j) * 2;
                int id = subData[dataIdx];
                int count = subData[dataIdx + 1];
                if (id == -2) {
                    std::ostringstream content;
                    content << std::setw(idxWidth) << cellIdx << ":?";
                    int pad = (idxWidth + 4) - (int)content.tellp();
                    for (int i = 0; i < pad; i++) content << " ";
                    std::cout << "{" << content.str() << "} ";
                } else {
                    printCell(std::cout, cellIdx, id, count, -1, idxWidth);
                    std::cout << " ";
                }
            }
            std::cout << std::endl;
        }
    }
}
