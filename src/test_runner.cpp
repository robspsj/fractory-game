#include "test_runner.hpp"
#include "game/game_model.hpp"
#include "print_state.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string dirOf(const std::string &filepath) {
  auto pos = filepath.find_last_of("/\\");
  return (pos != std::string::npos) ? filepath.substr(0, pos + 1) : "./";
}

bool ModelTestRunner::runTest(const std::string &filepath, int gridLimit) {
  GameModel model;
  std::string initialState;
  bool emptyGrid = false;
  int gridSize = 3;
  std::vector<TestStep> steps = loadSteps(filepath, initialState, emptyGrid, gridSize);

  std::string dir = dirOf(filepath);
  Config cfg(42, 50, gridLimit);
  cfg.reactionsCsvPath = dir + "reactions.csv";
  cfg.spawnCsvPath = dir + "spawns.csv";
  cfg.stationRecipesCsvPath = dir + "station_recipes.csv";
  cfg.emptyGrid = emptyGrid;
  cfg.initialGridSize = gridSize;
  model.init(cfg);
  if (!initialState.empty()) {
    loadState(model, initialState);
  }

  std::cout << "Initial state:" << std::endl;
  printState(model);

  for (const auto &step : steps) {
    if (step.type == "expect_cell") {
      int idx = model.rootChild(step.row, step.col);
      const auto &n = model.node(idx);
      int actualId;
      int actualCount;
      if (n.content.type == CellType::ITEM) {
        actualId = n.content.data.item.id;
        actualCount = n.content.data.item.count;
      } else if (n.content.type == CellType::GRID) {
        actualId = -2;
        actualCount = -1;
      } else if (n.content.type == CellType::STATION) {
        actualId = -3;
        actualCount = 0;
      } else if (n.content.type == CellType::RESERVED) {
        actualId = -4;
        actualCount = 0;
      } else {
        actualId = -1;
        actualCount = 0;
      }
      if (actualId != step.expectedId || actualCount != step.expectedCount) {
        std::cerr << "Expectation FAILED: Cell [" << step.row << "," << step.col
                  << "] expected [" << step.expectedId << ":"
                  << step.expectedCount << "] but got [" << actualId << ":"
                  << actualCount << "]" << std::endl;
        return false;
      } else {
        std::cout << "Expectation PASSED: Cell [" << step.row << "," << step.col
                  << "] is [" << actualId << ":" << actualCount << "]"
                  << std::endl;
      }
    } else if (step.type == "expect_drag") {
      int actualId = model.dragItemId();
      int actualCount = model.dragAmount();
      if (actualId != step.expectedId || actualCount != step.expectedCount) {
        std::cerr << "Expectation FAILED: Drag state expected ["
                  << step.expectedId << ":" << step.expectedCount
                  << "] but got [" << actualId << ":" << actualCount << "]"
                  << std::endl;
        return false;
      } else {
        std::cout << "Expectation PASSED: Drag state is [" << actualId << ":"
                  << actualCount << "]" << std::endl;
      }
    } else if (step.type == "pickup") {
      std::cout << "Action: pick up from [" << step.row << "," << step.col
                << "]" << std::endl;
      int idx = model.rootChild(step.row, step.col);
      if (model.node(idx).content.type == CellType::ITEM) {
        model.pickUp(idx, model.node(idx).content.data.item.count);
      }
      printState(model);
    } else if (step.type == "drop") {
      std::cout << "Action: drop at [" << step.row << "," << step.col << "]"
                << std::endl;
      model.drop(model.rootChild(step.row, step.col));
      printState(model);
    } else if (step.type == "cancel") {
      std::cout << "Action: cancel drag" << std::endl;
      model.cancelDrag();
      printState(model);
    } else if (step.type == "tick") {
      for (int t = 0; t < step.count; t++)
        model.tick();
      std::cout << "Action: tick x" << step.count << std::endl;
      printState(model);
    } else if (step.type == "interact") {
      std::cout << "Action: interact at [" << step.row << "," << step.col
                << "]" << std::endl;
      int idx = model.rootChild(step.row, step.col);
      model.interact(idx);
      printState(model);
    } else if (step.type == "place_station") {
      int idx = model.rootChild(step.row, step.col);
      bool ok = model.placeStation(idx, step.expectedId);
      std::cout << "Action: place_station at [" << step.row << "," << step.col
                << "] recipe=" << step.expectedId << (ok ? " OK" : " FAIL") << std::endl;
      printState(model);
    } else if (step.type == "add_input") {
      int idx = model.rootChild(step.row, step.col);
      Cell &cell = model.node(idx);
      if (cell.content.type == CellType::RESERVED) {
        cell.content.data.reserved.itemId = step.expectedId;
        cell.content.data.reserved.itemCount = step.expectedCount;
        // Trigger station start check
        model.tryStartStation(cell.content.data.reserved.anchorIndex);
      }
      std::cout << "Action: add_input at [" << step.row << "," << step.col
                << "] item=" << step.expectedId << " count=" << step.expectedCount << std::endl;
    } else if (step.type == "expect_reserved") {
      int idx = model.rootChild(step.row, step.col);
      const Cell &n = model.node(idx);
      if (n.content.type == CellType::RESERVED) {
        int actualId = n.content.data.reserved.itemId;
        int actualCount = n.content.data.reserved.itemCount;
        if (actualId != step.expectedId || actualCount != step.expectedCount) {
          std::cerr << "Expectation FAILED: Reserved [" << step.row << "," << step.col
                    << "] expected [" << step.expectedId << ":" << step.expectedCount
                    << "] but got [" << actualId << ":" << actualCount << "]" << std::endl;
          return false;
        } else {
          std::cout << "Expectation PASSED: Reserved [" << step.row << "," << step.col
                    << "] is [" << actualId << ":" << actualCount << "]" << std::endl;
        }
      } else {
        std::cerr << "Expectation FAILED: Cell [" << step.row << "," << step.col
                  << "] is not RESERVED (type=" << (int)n.content.type << ")" << std::endl;
        return false;
      }
    }
  }

  return true;
}

void ModelTestRunner::loadState(GameModel &model, const std::string &stateStr) {
  int gridData[GameModel::GRID * GameModel::GRID * 2];
  std::stringstream ss(stateStr);
  std::string segment;
  int i = 0;
  while (std::getline(ss, segment, ',') &&
         i < GameModel::GRID * GameModel::GRID) {
    size_t colon = segment.find(':');
    int id = std::stoi(segment.substr(0, colon));
    int count = std::stoi(segment.substr(colon + 1));
    gridData[i * 2] = id;
    gridData[i * 2 + 1] = count;
    i++;
  }
  model.setFullState(gridData);
}

std::vector<TestStep> ModelTestRunner::loadSteps(const std::string &filepath,
                                                 std::string &outInitialState,
                                                 bool &outEmptyGrid,
                                                 int &outGridSize) {
  std::vector<TestStep> steps;
  std::ifstream file(filepath);
  std::string line;

  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    if (line.rfind("init:", 0) == 0) {
      outInitialState = line.substr(5);
      continue;
    }

    if (line.rfind("empty_grid:", 0) == 0) {
      outEmptyGrid = true;
      outGridSize = std::stoi(line.substr(11));
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
    } else if (type == "cancel") {
      TestStep step;
      step.type = "cancel";
      steps.push_back(step);
    } else if (type == "tick") {
      TestStep step;
      step.type = "tick";
      std::string count_str;
      if (std::getline(ss, count_str, ',') && !count_str.empty())
        step.count = std::stoi(count_str);
      steps.push_back(step);
    } else if (type == "interact") {
      std::string row_str, col_str;
      std::getline(ss, row_str, ',');
      std::getline(ss, col_str, ',');
      TestStep step;
      step.type = "interact";
      step.row = std::stoi(row_str);
      step.col = std::stoi(col_str);
      steps.push_back(step);
    } else if (type == "place_station") {
      std::string row_str, col_str, recipe_str;
      std::getline(ss, row_str, ',');
      std::getline(ss, col_str, ',');
      std::getline(ss, recipe_str, ',');
      TestStep step;
      step.type = "place_station";
      step.row = std::stoi(row_str);
      step.col = std::stoi(col_str);
      step.expectedId = std::stoi(recipe_str);
      steps.push_back(step);
    } else if (type == "add_input") {
      std::string row_str, col_str, id_str, cnt_str;
      std::getline(ss, row_str, ',');
      std::getline(ss, col_str, ',');
      std::getline(ss, id_str, ',');
      std::getline(ss, cnt_str, ',');
      TestStep step;
      step.type = "add_input";
      step.row = std::stoi(row_str);
      step.col = std::stoi(col_str);
      step.expectedId = std::stoi(id_str);
      step.expectedCount = std::stoi(cnt_str);
      steps.push_back(step);
    } else if (type == "expect_reserved") {
      std::string row_str, col_str, id_str, cnt_str;
      std::getline(ss, row_str, ',');
      std::getline(ss, col_str, ',');
      std::getline(ss, id_str, ',');
      std::getline(ss, cnt_str, ',');
      TestStep step;
      step.type = "expect_reserved";
      step.row = std::stoi(row_str);
      step.col = std::stoi(col_str);
      step.expectedId = std::stoi(id_str);
      step.expectedCount = std::stoi(cnt_str);
      steps.push_back(step);
    }
  }
  return steps;
}
