#pragma once
#include <string>
#include <vector>

struct TestStep {
  std::string type;
  int row = 0;
  int col = 0;
  int expectedId = -1;
  int expectedCount = 0;
};

class ModelTestRunner {
public:
  static bool runTest(const std::string &filepath, int gridLimit = 100);

private:
  static void loadState(class GameModel &model, const std::string &stateStr);
  static std::vector<TestStep> loadSteps(const std::string &filepath,
                                         std::string &outInitialState);
};
