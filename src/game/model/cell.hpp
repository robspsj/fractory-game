#pragma once
#include "../config.hpp"
#include "../interact_config.hpp"
#include <cstdint>

enum class CellType { EMPTY, ITEM, GRID, STATION, RESERVED };

enum class CellEventType { REACTION, SPAWN, SPILL, DESTROY, STATION_OUTPUT, GROW, SHRINK };
struct CellEvent {
  int nodeIndex;
  int itemId;
  CellEventType type;
};

struct ItemData {
  int id;
  int count;
};

struct GridData {
  int firstChild;
  int gridDimension;
  int variation;
};

struct StationData {
  int recipeIndex;
  int progress;
  int sizeR, sizeC;
};

struct ReservedData {
  int anchorIndex;
  ReserveRole role;
  int itemId;
  int itemCount;
};

// Type is tracked separately (GameModel::_type, parallel to the node array)
// and byteSize was fully derivable from the type, so CellContent is now just
// the payload union. 24-byte cells instead of 32.
struct CellContent {
  union {
    ItemData item;
    GridData grid;
    StationData station;
    ReservedData reserved;
  } data;

  CellContent() { data.item = {-1, 0}; }
};

struct Cell {
  int parentId = -1;
  CellContent content;
  uint8_t heat = 0; // 0-255, only meaningful when GameModel::typeOf() != GRID
};
