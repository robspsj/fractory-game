#include "reserved_cell.hpp"
#include "game_model.hpp"
#include "station_cell.hpp"

void ReservedCell::interact(int idx, GameModel &model) {
  ReservedData &rd = model._nodes[idx].content.data.reserved;
  if (rd.role == ReserveRole::OUTPUT && rd.itemId != -1) {
    model._dragSrcIndex = idx;
    model._dragItemId = rd.itemId;
    model._dragAmount = rd.itemCount;
    rd.itemId = -1;
    rd.itemCount = 0;
  } else if (rd.role == ReserveRole::INPUT && model.hasDrag()) {
    if (rd.itemId == -1) {
      rd.itemId = model._dragItemId;
      rd.itemCount = model._dragAmount;
      model._dragSrcIndex = -1;
      model._dragAmount = 0;
      model._dragItemId = -1;
      StationCell::tryStart(rd.anchorIndex, model);
    }
  } else if (rd.role == ReserveRole::BUFFER) {
    if (model.hasDrag() && rd.itemId == -1) {
      rd.itemId = model._dragItemId;
      rd.itemCount = model._dragAmount;
      model._dragSrcIndex = -1;
      model._dragAmount = 0;
      model._dragItemId = -1;
    } else if (!model.hasDrag() && rd.itemId != -1) {
      model._dragSrcIndex = idx;
      model._dragItemId = rd.itemId;
      model._dragAmount = rd.itemCount;
      rd.itemId = -1;
      rd.itemCount = 0;
    }
  }
}

void ReservedCell::tick(int, GameModel &) {
  // No autonomous tick
}
