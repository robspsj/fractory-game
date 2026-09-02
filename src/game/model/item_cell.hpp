#pragma once
#include <vector>
#include "cell.hpp"
struct PlMut;
struct PlSpill;
class GameModel;

class ItemCell {
 public:
  // Called from GameModel::tick()'s parallel phase: decides grow/shrink/spill
  // for a single ITEM cell and appends to the thread-local buffers.
  static void collectTickPlan(int idx, const Cell &cell, GameModel &model,
                              std::vector<PlMut> &muts, std::vector<PlSpill> &spills,
                              double &spillMs, long &itemCounter);
  static void interact(int idx, GameModel &model);
  static void tick(int idx, GameModel &model);
  static int findSpillTarget(int idx, GameModel &model);
};
