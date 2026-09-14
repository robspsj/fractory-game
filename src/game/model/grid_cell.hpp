#pragma once
#include <vector>
#include "cell.hpp"
class GameModel;

class GridCell {
 public:
  static void interact(int idx, GameModel &model);
  static void tick(int idx, GameModel &model);
  // Grid-specific topology helper — kept here so GameModel doesn't own raw layout math.
  static void populateWithSubgrid(int cellIndex, int size, std::vector<Cell> &nodes,
                                  std::vector<uint8_t> &types);
};
