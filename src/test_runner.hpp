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
        
        for (const auto& action : actions) {
            if (action.type == "mousedown") {
                gameMouseDown(action.x, action.y);
            } else if (action.type == "mouseup") {
                gameMouseUp(action.x, action.y);
            }
        }
        
        // Output final state for verification
        int* state = gameGetState();
        std::cout << "Test completed. Final grid state:" << std::endl;
        for(int i = 0; i < 25; ++i) {
            std::cout << state[i] << (i % 5 == 4 ? "\n" : " ");
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
