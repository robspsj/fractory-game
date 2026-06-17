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

static void printCell(std::ostream& os, int arrayIdx, int id, int count, int subgridIdx, int idxWidth) {
    int contentWidth = idxWidth + 4;
    std::ostringstream content;
    content << std::setw(idxWidth) << arrayIdx;
    if (id == -2) {
        content << ":#:" << (char)('a' + subgridIdx);
    } else if (id == -1) {
        content << ":";
        int pad = contentWidth - (int)content.tellp();
        for (int i = 0; i < pad; i++) content << "_";
    } else {
        content << ":" << id << ":" << count;
    }
    if (id == -2) {
        os << "{" << content.str() << "}";
    } else {
        os << "[" << content.str() << "]";
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
        std::cout << "Drag:  [" << dragId << ":" << dragAmount << "]" << std::endl;
    } else {
        std::cout << "Drag:  None" << std::endl;
    }

    int maxSubgridIdx = -1;
    int first = 1;
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
        std::cout << std::endl << "Subgrid " << letter << ":" << std::endl;
        int subData[9 * 2];
        int subSize;
        int subFirst = model.getSubgridState(s, subData, subSize);
        for (int i = subSize - 1; i >= 0; --i) {
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
