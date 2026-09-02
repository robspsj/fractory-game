#pragma once
#include "cell.hpp"

struct PlMut { int idx; CellType type; int item; int count; CellEventType ev; };
struct PlSpill { int src, tgt, item; };
