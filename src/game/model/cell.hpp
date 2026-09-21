#pragma once
#include "../config.hpp"
#include "../interact_config.hpp"
#include <cstdint>

enum class CellType { EMPTY, ITEM, GRID, STATION, RESERVED };

enum class CellEventType { REACTION, SPAWN, SPILL, DESTROY, STATION_OUTPUT, GROW, SHRINK, QUANTUM_SPAWN };
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
  uint16_t progress;      // ticks remaining (recipe duration clamped on store)
  uint16_t recipeIndex;
  uint8_t sizeR, sizeC;   // footprint from the recipe
};

// Sentinel for 'no item' in the uint16 item-id fields (ids can then use
// the full 0..65534 range).
inline constexpr uint16_t NO_ITEM = 0xFFFF;

struct ReservedData {
  // The footprint always lies in the same grid as the anchor, below-right of
  // it, so a 16-bit delta is provably enough: cell - anchor < dim*dim <= 255².
  uint16_t anchorDelta;   // cellIndex - anchorIndex  (anchor = cellIndex - delta)
  ReserveRole role;
  uint16_t itemId;        // NO_ITEM == empty
  uint16_t itemCount;
};

// Type is tracked separately (GameModel::_type, parallel to the node array)
// and byteSize was fully derivable from the type, so CellContent is just the
// payload union: largest variant 8 bytes (Grid/Reserved), 4-aligned ->
// Cell = 4+8+1 -> 16 bytes.
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
