#pragma once
class GameModel;

class ReservedCell {
 public:
  static void interact(int idx, GameModel &model);
  static void tick(int idx, GameModel &model);
};
