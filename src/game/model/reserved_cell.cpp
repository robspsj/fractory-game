#include "reserved_cell.hpp"
#include "game_model.hpp"
#include "station_cell.hpp"
#include "../heat_constants.hpp"

void ReservedCell::interact(int idx, GameModel &model) {
  ReservedData &rd = model._nodes[idx].content.data.reserved;
  if (rd.role == ReserveRole::OUTPUT && rd.itemId != -1) {
    model._dragSrcIndex = idx;
    model._dragItemId = rd.itemId;
    model._dragAmount = rd.itemCount;
    rd.itemId = -1;
    rd.itemCount = 0;
    model.addHeat(idx, Heat::STATION_ANCHOR_HEAT);
  } else if (rd.role == ReserveRole::INPUT && model.hasDrag()) {
    if (rd.itemId == -1) {
      rd.itemId = model._dragItemId;
      rd.itemCount = model._dragAmount;
      model.addHeat(idx, Heat::STATION_INPUT_HEAT);
      model.addHeat(rd.anchorIndex, Heat::STATION_ANCHOR_HEAT);
      model._dragSrcIndex = -1;
      model._dragAmount = 0;
      model._dragItemId = -1;
      StationCell::tryStart(rd.anchorIndex, model);
    }
  } else if (rd.role == ReserveRole::BUFFER) {
    if (model.hasDrag() && rd.itemId == -1) {
      rd.itemId = model._dragItemId;
      rd.itemCount = model._dragAmount;
      model.addHeat(idx, Heat::RESERVED_BUFFER_HEAT_IN);
      model._dragSrcIndex = -1;
      model._dragAmount = 0;
      model._dragItemId = -1;
    } else if (!model.hasDrag() && rd.itemId != -1) {
      model._dragSrcIndex = idx;
      model._dragItemId = rd.itemId;
      model._dragAmount = rd.itemCount;
      rd.itemId = -1;
      rd.itemCount = 0;
      model.addHeat(idx, Heat::RESERVED_BUFFER_HEAT_OUT);
    }
  }
}

void ReservedCell::tick(int, GameModel &) {
  // No autonomous tick
}
