#include "empty_cell.hpp"
#include "game_model.hpp"

void EmptyCell::interact(int idx, GameModel &model) {
  Cell &cell = model._nodes[idx];
  const SpawnRule *rule = model._interactCfg.pickSpawn(model._dragItemId);
  if (rule) {
    cell.content.type = CellType::ITEM;
    cell.content.data.item.id = rule->spawnItemId;
    cell.content.data.item.count = rule->spawnCount;
    cell.content.byteSize = calcByteSize(CellType::ITEM);
    model.emitEvent(CellEventType::SPAWN, idx, rule->spawnItemId);
    model.addHeat(idx, 10);
  }
}

void EmptyCell::tick(int, GameModel &) {
  // No-op for now
}
