#pragma once
#include <vector>
class GameModel;

class StationCell {
 public:
  static void interact(int idx, GameModel &model);
  static void tick(int idx, GameModel &model);
  static void tryStart(int anchorIdx, GameModel &model);
  static bool place(int gridIndex, int recipeId, GameModel &model);
  static void remove(int anchorIndex, GameModel &model);
  // Used by the parallel tick phase — stations are only collected, ticked serially.
  static void collectTickPlan(int idx, GameModel &model, std::vector<int> &stations);
};
