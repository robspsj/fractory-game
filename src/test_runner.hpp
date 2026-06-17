#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include "game/game_model.hpp"

struct TestStep {
    std::string type;
    int row = 0;
    int col = 0;
    int expectedId = -1;
    int expectedCount = 0;
};

class ModelTestRunner {
public:
    static bool runTest(const std::string& filepath) {
        GameModel model;
        std::string initialState;
        std::vector<TestStep> steps = loadSteps(filepath, initialState);

        model.init(42);
        if (!initialState.empty()) {
            loadState(model, initialState);
        }

        std::cout << "Initial state:" << std::endl;
        printState(model);

        for (const auto& step : steps) {
            if (step.type == "expect_cell") {
                int idx = model.rootChild(step.row, step.col);
                const auto& n = model.node(idx);
                int actualId;
                int actualCount;
                if (n.type == CellType::ITEM) {
                    actualId = n.data.item.id;
                    actualCount = n.data.item.count;
                } else if (n.type == CellType::GRID) {
                    actualId = -2;
                    actualCount = -1;
                } else {
                    actualId = -1;
                    actualCount = 0;
                }
                if (actualId != step.expectedId || actualCount != step.expectedCount) {
                    std::cerr << "Expectation FAILED: Cell [" << step.row << "," << step.col << "] expected ["
                              << step.expectedId << ":" << step.expectedCount << "] but got ["
                              << actualId << ":" << actualCount << "]" << std::endl;
                    return false;
                } else {
                    std::cout << "Expectation PASSED: Cell [" << step.row << "," << step.col << "] is ["
                              << actualId << ":" << actualCount << "]" << std::endl;
                }
            } else if (step.type == "expect_drag") {
                int actualId = model.dragItemId();
                int actualCount = model.dragAmount();
                if (actualId != step.expectedId || actualCount != step.expectedCount) {
                    std::cerr << "Expectation FAILED: Drag state expected ["
                              << step.expectedId << ":" << step.expectedCount << "] but got ["
                              << actualId << ":" << actualCount << "]" << std::endl;
                    return false;
                } else {
                    std::cout << "Expectation PASSED: Drag state is ["
                              << actualId << ":" << actualCount << "]" << std::endl;
                }
            } else if (step.type == "pickup") {
                std::cout << "Action: pick up from [" << step.row << "," << step.col << "]" << std::endl;
                int idx = model.rootChild(step.row, step.col);
                if (model.node(idx).type == CellType::ITEM) {
                    model.pickUp(idx, model.node(idx).data.item.count);
                }
                printState(model);
            } else if (step.type == "drop") {
                std::cout << "Action: drop at [" << step.row << "," << step.col << "]" << std::endl;
                model.drop(model.rootChild(step.row, step.col));
                printState(model);
            }
        }

        return true;
    }

private:
    static void printState(const GameModel& model) {
        int data[GameModel::GRID * GameModel::GRID * 2];
        model.getFullState(data);

        int dragId = model.dragItemId();
        int dragAmount = model.dragAmount();

        if (dragId != -1) {
            std::cout << "Dragging: [" << dragId << ":" << dragAmount << "]" << std::endl;
        } else {
            std::cout << "Dragging: None" << std::endl;
        }

        int maxSubgridIdx = -1;
        for (int i = 0; i < GameModel::GRID; ++i) {
            for (int j = 0; j < GameModel::GRID; ++j) {
                int id = data[(i * GameModel::GRID + j) * 2];
                int count = data[(i * GameModel::GRID + j) * 2 + 1];
                if (id == -2) {
                    char letter = 'a' + count;
                    if (count > maxSubgridIdx) maxSubgridIdx = count;
                    std::cout << "[s:" << letter << "] ";
                } else if (id == -1) {
                    std::cout << "[___] ";
                } else {
                    std::cout << "[" << id << ":" << count << "] ";
                }
            }
            std::cout << std::endl;
        }

        for (int s = 0; s <= maxSubgridIdx; s++) {
            char letter = 'a' + s;
            std::cout << std::endl << "Subgrid " << letter << ":" << std::endl;
            int subData[9 * 2];
            int subSize;
            model.getSubgridState(s, subData, subSize);
            for (int i = 0; i < subSize; i++) {
                for (int j = 0; j < subSize; j++) {
                    int idx = i * subSize + j;
                    int id = subData[idx * 2];
                    int count = subData[idx * 2 + 1];
                    if (id == -2) {
                        std::cout << "[s:_] ";
                    } else if (id == -1) {
                        std::cout << "[___] ";
                    } else {
                        std::cout << "[" << id << ":" << count << "] ";
                    }
                }
                std::cout << std::endl;
            }
        }
    }

    static void loadState(GameModel& model, const std::string& stateStr) {
        int gridData[GameModel::GRID * GameModel::GRID * 2];
        std::stringstream ss(stateStr);
        std::string segment;
        int i = 0;
        while (std::getline(ss, segment, ',') && i < GameModel::GRID * GameModel::GRID) {
            size_t colon = segment.find(':');
            int id = std::stoi(segment.substr(0, colon));
            int count = std::stoi(segment.substr(colon + 1));
            gridData[i * 2] = id;
            gridData[i * 2 + 1] = count;
            i++;
        }
        model.setFullState(gridData);
    }

    static std::vector<TestStep> loadSteps(const std::string& filepath, std::string& outInitialState) {
        std::vector<TestStep> steps;
        std::ifstream file(filepath);
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            if (line.rfind("init:", 0) == 0) {
                outInitialState = line.substr(5);
                continue;
            }

            std::stringstream ss(line);
            std::string type;
            std::getline(ss, type, ',');

            if (type == "expect_cell") {
                std::string row_str, col_str, id_str, count_str;
                std::getline(ss, row_str, ',');
                std::getline(ss, col_str, ',');
                std::getline(ss, id_str, ',');
                std::getline(ss, count_str, ',');

                TestStep step;
                step.type = "expect_cell";
                step.row = std::stoi(row_str);
                step.col = std::stoi(col_str);
                step.expectedId = std::stoi(id_str);
                step.expectedCount = std::stoi(count_str);
                steps.push_back(step);
            } else if (type == "expect_drag") {
                std::string id_str, count_str;
                std::getline(ss, id_str, ',');
                std::getline(ss, count_str, ',');

                TestStep step;
                step.type = "expect_drag";
                step.expectedId = std::stoi(id_str);
                step.expectedCount = std::stoi(count_str);
                steps.push_back(step);
            } else if (type == "pickup" || type == "drop") {
                std::string row_str, col_str;
                std::getline(ss, row_str, ',');
                std::getline(ss, col_str, ',');

                TestStep step;
                step.type = type;
                step.row = std::stoi(row_str);
                step.col = std::stoi(col_str);
                steps.push_back(step);
            }
        }
        return steps;
    }
};
