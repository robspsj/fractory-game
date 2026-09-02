#pragma once
#include "types.hpp"
class GameView;

class BadgeRenderer {
 public:
  static void drawCountBadge(GameView &view, const Rect &r, int count);
};
