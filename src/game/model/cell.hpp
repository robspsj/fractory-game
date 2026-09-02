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

struct CellContent {
  CellType type = CellType::EMPTY;
  int byteSize = 0;
  union {
    ItemData item;
    GridData grid;
    StationData station;
    ReservedData reserved;
  } data;

  CellContent() : type(CellType::EMPTY), byteSize(0) {
    data.item = {-1, 0};
  }
};

struct Cell {
  int parentId = -1;
  CellContent content;
  uint8_t heat = 0; // 0-255, only meaningful when content.type != GRID (composite)
};

inline int calcByteSize(CellType type) {
  switch (type) {
    case CellType::EMPTY:    return 10; // +1 heat
    case CellType::ITEM:     return 14;
    case CellType::GRID:     return 13; // composite: no heat
    case CellType::STATION:  return 18;
    case CellType::RESERVED: return 19;
  }
  return 0;
}
