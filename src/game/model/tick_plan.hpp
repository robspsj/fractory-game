#pragma once
#include "cell.hpp"

struct PlMut { int idx; CellType type; int item; int count; CellEventType ev; };
// amount = how many units move to tgt; the source keeps count-amount, so a
// split moves HALF the stack (see Heat::ITEM_SPILL_*).
struct PlSpill { int src, tgt, item, amount; };
