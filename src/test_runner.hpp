#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include "game/game.hpp"

struct TestAction {
    std::string type;
    int x;
    int y;
};

class TestRunner {
public:
    static bool runTest(const std::string& filepath) {
        std::vector<TestAction> actions = loadActions(filepath);
        
        gameInit();
        
        // Use default window size for testing
        int testW = 800;
        int testH = 600;
        
        for (const auto& action : actions) {
            if (action.type == "mousedown") {
                gameMouseDown(action.x, action.y, testW, testH);
            } else if (action.type == "mouseup") {
                gameMouseUp(action.x, action.y, testW, testH);
            }
        }
        
        // Output final state for verification
        int grid[5][5];
        gameGetState(grid);
        std::cout << "Test completed. Final grid state:" << std::endl;
        for(int i = 0; i < 5; ++i) {
            for(int j = 0; j < 5; ++j) {
                std::cout << grid[i][j] << (j == 4 ? "" : " ");
            }
            std::cout << std::endl;
        }
        
        return true;
    }

private:
    static std::vector<TestAction> loadActions(const std::string& filepath) {
        std::vector<TestAction> actions;
        std::ifstream file(filepath);
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::stringstream ss(line);
            std::string type, x_str, y_str;
            
            std::getline(ss, type, ',');
            std::getline(ss, x_str, ',');
            std::getline(ss, y_str, ',');
            
            actions.push_back({type, std::stoi(x_str), std::stoi(y_str)});
        }
        return actions;
    }
};
