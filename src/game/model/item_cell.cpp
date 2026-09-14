#include "item_cell.hpp"
#include "game_model.hpp"
#include "tick_plan.hpp"
#include "../heat_constants.hpp"
#include <algorithm>
#include <chrono>

int ItemCell::findSpillTarget(int idx, GameModel &model) {
  std::vector<int> candidates;
  // Precomputed world-space adjacency (one deepest-leaf neighbour per
  // direction, crossing subgrid boundaries where the layout touches).
  // Iteration order up,down,left,right matches the legacy candidate order.
  const Adjacency &a = model._adj[idx];
  const int dirs[4] = {a.n[ADJ_UP], a.n[ADJ_DOWN], a.n[ADJ_LEFT], a.n[ADJ_RIGHT]};
  for (int ni : dirs) {
    if (ni >= 0 && ni < (int)model._nodes.size() &&
        model.typeOf(ni) == CellType::EMPTY) {
      candidates.push_back(ni);
    }
  }
  if (candidates.empty()) return -1;
  return candidates[(int)(model.prng(idx, 2) % (uint32_t)candidates.size())];
}

void ItemCell::collectTickPlan(int idx, const Cell &cell, GameModel &model,
                               std::vector<PlMut> &muts, std::vector<PlSpill> &spills,
                               double &spillMs, long &itemCounter) {
  using clk = std::chrono::steady_clock;
  itemCounter++;
  const int item = cell.content.data.item.id;
  const int cnt = cell.content.data.item.count;
  int heat = cell.heat;
  int freqDenom = Heat::ITEM_FREQ_BASE - (heat * Heat::ITEM_FREQ_RANGE / 255);
  if (freqDenom < Heat::ITEM_FREQ_MIN) freqDenom = Heat::ITEM_FREQ_MIN;
  if (freqDenom > Heat::ITEM_FREQ_MAX) freqDenom = Heat::ITEM_FREQ_MAX;
  if (model.prng(idx, 0) % (uint32_t)freqDenom != 0) return;
  if (model.prng(idx, 1) & 1) {
    if (cnt > 1)
      muts.push_back({idx, CellType::ITEM, item, cnt - 1, CellEventType::SHRINK});
    else
      muts.push_back({idx, CellType::EMPTY, item, 0, CellEventType::DESTROY});
  } else {
    if (cnt < 5)
      muts.push_back({idx, CellType::ITEM, item, cnt + 1, CellEventType::GROW});
    else {
      auto s = clk::now();
      int tgt = findSpillTarget(idx, model);
      spillMs += std::chrono::duration<double, std::milli>(clk::now() - s).count();
      if (tgt != -1) spills.push_back({idx, tgt, item});
    }
  }
}

void ItemCell::interact(int idx, GameModel &model) {
  Cell &cell = model._nodes[idx];
  // Precomputed adjacency: same order as the legacy {up,down,left,right}
  // scan, but neighbours may live inside an adjacent subgrid (the old
  // same-grid-only lookup could not target cells across hierarchy edges).
  const Adjacency &a = model._adj[idx];
  const int dirs[4] = {a.n[ADJ_UP], a.n[ADJ_DOWN], a.n[ADJ_LEFT], a.n[ADJ_RIGHT]};

  for (int nIdx : dirs) {
    if (nIdx < 0) continue;
    const Cell &neighbor = model._nodes[nIdx];
    if (model.typeOf(nIdx) != CellType::ITEM) continue;

    const ReactionRule *rule =
        model._interactCfg.findReaction(cell.content.data.item.id, neighbor.content.data.item.id);
    if (rule) {
      // (Legacy wrote EMPTY then ITEM on idx as a dance; setType makes the
      // final type atomic, so one write each is enough.)
      model.setType(nIdx, CellType::EMPTY);
      model.setType(idx, CellType::ITEM);
      cell.content.data.item.id = rule->result;
      cell.content.data.item.count = rule->resultCount;
      model.emitEvent(CellEventType::REACTION, idx, rule->result);
      model.addHeat(idx, Heat::INTERACT_REACTION_A);
      model.addHeat(nIdx, Heat::INTERACT_REACTION_B);
      return;
    }
  }
  cell.content.data.item.id = (cell.content.data.item.id + 1) % model._numItems;
  model.addHeat(idx, Heat::INTERACT_CYCLE);
}

void ItemCell::tick(int, GameModel &) {
  // Item tick is planned in parallel via collectTickPlan; serial part handled by GameModel.
}
