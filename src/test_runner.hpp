#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <SDL3/SDL.h>
#include "game/game.hpp"

struct TestStep {
    std::string type; // "mousedown", "mouseup", "expect"
    // Mouse coords for actions:
    int x = 0;
    int y = 0;
    // Expect parameters:
    std::string expectType; // "cell", "grab", "drag"
    int row = 0;
    int col = 0;
    int expectedId = -1;
    int expectedCount = 0;
};

class TestRunner {
public:
    static void printState() {
        int data[5 * 5 * 2];
        gameGetFullState(data);
        
        int dragId, dragAmount;
        gameGetDragState(dragId, dragAmount);
        
        // Print drag state if active
        if (dragId != -1) {
            std::cout << "Dragging: [" << dragId << ":" << dragAmount << "]" << std::endl;
        } else {
            std::cout << "Dragging: None" << std::endl;
        }

        for(int i = 0; i < 5; ++i) {
            for(int j = 0; j < 5; ++j) {
                int id = data[(i * 5 + j) * 2];
                int count = data[(i * 5 + j) * 2 + 1];
                if (id == -1) std::cout << "[___] ";
                else std::cout << "[" << id << ":" << count << "] ";
            }
            std::cout << std::endl;
        }
    }

    static bool runTest(const std::string& filepath) {
        std::string initialState;
        std::vector<TestStep> steps = loadSteps(filepath, initialState);
        
        gameInit(42);
        if (!initialState.empty()) {
            loadState(initialState);
        }
        
        // Use test window dimensions that align with our grid logic
        int testW = 800;
        int testH = 600;
        
        std::cout << "Initial state:" << std::endl;
        printState();
        
        for (const auto& step : steps) {
            if (step.type == "expect") {
                if (step.expectType == "cell") {
                    int data[5 * 5 * 2];
                    gameGetFullState(data);
                    int actualId = data[(step.row * 5 + step.col) * 2];
                    int actualCount = data[(step.row * 5 + step.col) * 2 + 1];
                    if (actualId != step.expectedId || actualCount != step.expectedCount) {
                        std::cerr << "Expectation FAILED: Cell [" << step.row << "," << step.col << "] expected [" 
                                  << step.expectedId << ":" << step.expectedCount << "] but got [" 
                                  << actualId << ":" << actualCount << "]" << std::endl;
                        return false;
                    } else {
                        std::cout << "Expectation PASSED: Cell [" << step.row << "," << step.col << "] is [" 
                                  << actualId << ":" << actualCount << "]" << std::endl;
                    }
                } else if (step.expectType == "grab" || step.expectType == "drag") {
                    int actualId, actualCount;
                    gameGetDragState(actualId, actualCount);
                    if (actualId != step.expectedId || actualCount != step.expectedCount) {
                        std::cerr << "Expectation FAILED: Grab state expected [" 
                                  << step.expectedId << ":" << step.expectedCount << "] but got [" 
                                  << actualId << ":" << actualCount << "]" << std::endl;
                        return false;
                    } else {
                        std::cout << "Expectation PASSED: Grab state is [" 
                                  << actualId << ":" << actualCount << "]" << std::endl;
                    }
                }
            } else {
                std::cout << "Action: " << step.type << " at " << step.x << "," << step.y << std::endl;
                
                // Map test coordinates for easier testing (centering 400,300 to grid)
                // A click at 400, 300 should hit the center of the screen, which 
                // is roughly where our grid is centered if the window is 800x600.
                // Let's use relative offsets from center 400,300.
                int adjX = step.x; 
                int adjY = step.y;
                // The grid coordinates map roughly from -0.75 to +0.75.
                // 800px width -> -1.0 to 1.0.
                // Let's adjust to try and hit the cell [1, 2] (row 1, col 2)
                // Cell (row 1, col 2) is at roughly:
                // x = gridMin + 2 * cellSize + halfSize = -0.75 + 0.6 + 0.15 = 0.0
                // y = gridMin + 1 * cellSize + halfSize = -0.75 + 0.3 + 0.15 = -0.3
                // In pixel coordinates (800x600):
                // nx = (2 * x / 800) - 1 => 0 = (2 * px / 800) - 1 => px = 400
                // ny = (1 - 2 * y / 600) / aspect => -0.3 = (1 - 2 * py / 600) / (800/600)
                // -0.3 * 1.33 = 1 - 2*py/600 => -0.4 = 1 - 2*py/600 => 1.4 = 2*py/600 => py = 420
                
                if (step.type == "mousedown") {
                    gameMouseDown(SDL_BUTTON_LEFT, 400, 420, testW, testH);
                } else if (step.type == "mouseup") {
                    gameMouseUp(SDL_BUTTON_LEFT, 480, 420, testW, testH);
                }
                printState();
            }
        }
        
        return true;
    }

private:
    static void loadState(const std::string& stateStr) {
        // stateStr format: "1:2,0:5,-1:0,..." (id:count,id:count,...)
        int gridData[5 * 5 * 2];
        std::stringstream ss(stateStr);
        std::string segment;
        int i = 0;
        while (std::getline(ss, segment, ',') && i < 5 * 5) {
            size_t colon = segment.find(':');
            int id = std::stoi(segment.substr(0, colon));
            int count = std::stoi(segment.substr(colon + 1));
            gridData[i * 2] = id;
            gridData[i * 2 + 1] = count;
            i++;
        }
        gameSetFullState(gridData);
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
            
            if (type == "expect") {
                std::string expectType;
                std::getline(ss, expectType, ',');
                if (expectType == "cell") {
                    std::string row_str, col_str, id_str, count_str;
                    std::getline(ss, row_str, ',');
                    std::getline(ss, col_str, ',');
                    std::getline(ss, id_str, ',');
                    std::getline(ss, count_str, ',');
                    
                    TestStep step;
                    step.type = "expect";
                    step.expectType = "cell";
                    step.row = std::stoi(row_str);
                    step.col = std::stoi(col_str);
                    step.expectedId = std::stoi(id_str);
                    step.expectedCount = std::stoi(count_str);
                    steps.push_back(step);
                } else if (expectType == "grab" || expectType == "drag") {
                    std::string id_str, count_str;
                    std::getline(ss, id_str, ',');
                    std::getline(ss, count_str, ',');
                    
                    TestStep step;
                    step.type = "expect";
                    step.expectType = expectType;
                    step.expectedId = std::stoi(id_str);
                    step.expectedCount = std::stoi(count_str);
                    steps.push_back(step);
                }
            } else {
                std::string x_str, y_str;
                std::getline(ss, x_str, ',');
                std::getline(ss, y_str, ',');
                
                TestStep step;
                step.type = type;
                step.x = std::stoi(x_str);
                step.y = std::stoi(y_str);
                steps.push_back(step);
            }
        }
        return steps;
    }
};