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
  uint16_t id;     // element id (0..ELEMS-1)
  uint16_t count;  // clamped to 65535 on merge
};

struct GridData {
  int32_t firstChild;
  uint8_t gridDimension;  // cells per side (2..7 today)
  uint8_t variation;      // tile-art family
};

struct StationData {
  int32_t progress;       // ticks remaining (declared first: packs with no padding)
  uint16_t recipeIndex;
  uint8_t sizeR, sizeC;   // footprint from the recipe (<=7 today)
};

struct ReservedData {
  int32_t anchorIndex;    // the STATION cell it belongs to
  ReserveRole role;
  int16_t itemId;         // -1 == empty (hence signed)
  uint16_t itemCount;
};

// Type is tracked separately (GameModel::_type, parallel to the node array)
// and byteSize was fully derivable from the type, so CellContent is just the
// payload union: largest variant 12 bytes (ReservedData) -> Cell = 4+12+1 ->
// 20 bytes with tail padding.
struct CellContent {
  union {
    ItemData item;
    GridData grid;
    StationData station;
    ReservedData reserved;
  } data;

  CellContent() { data.item = {0, 0}; }
};

struct Cell {
  int parentId = -1;
  CellContent content;
  uint8_t heat = 0; // 0-255, only meaningful when GameModel::typeOf() != GRID
};
