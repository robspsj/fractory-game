#include "item_cell.hpp"
#include "game_model.hpp"
#include "tick_plan.hpp"
#include "../heat_constants.hpp"
#include <algorithm>
#include <chrono>

int ItemCell::findSpillTarget(int idx, GameModel &model) {
  // At most one candidate per direction (precomputed adjacency) — stack
  // array, no heap (this runs on the plan path thousands of times/tick).
  int cand[4];
  int nc = 0;
  const Adjacency &a = model._adj[idx];
  const int dirs[4] = {a.n[ADJ_UP], a.n[ADJ_DOWN], a.n[ADJ_LEFT], a.n[ADJ_RIGHT]};
  for (int ni : dirs) {
    if (ni >= 0 && ni < (int)model._nodes.size() &&
        model.typeOf(ni) == CellType::EMPTY) {
      cand[nc++] = ni;
    }
  }
  if (nc == 0) return -1;
  return cand[(int)(model.prng(idx, 2) % (uint32_t)nc)];
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
    // Decrease side. Feature flag (Config::itemShrink): off by default, so the
    // board does not lose items on its own — the code path stays intact.
    if (!model.itemShrink()) return;
    if (cnt > 1)
      muts.push_back({idx, CellType::ITEM, item, cnt - 1, CellEventType::SHRINK});
    else
      muts.push_back({idx, CellType::EMPTY, item, 0, CellEventType::DESTROY});
  } else {
    // Increase side — no hard cap. A stack past ITEM_SPILL_COMFORT may split
    // instead, with a chance that rises with the stack; the split takes half
    // (proportional drain). Reaching the uint16 ceiling forces a split.
    // Both halves are feature flags (Config::itemSplit / itemGrow).
    int amount = 0;
    if (model.itemSplit()) {
      if (cnt >= Heat::ITEM_COUNT_MAX) {
        amount = cnt / 2;
      } else if (cnt > Heat::ITEM_SPILL_COMFORT) {
        int pct = Heat::ITEM_SPILL_CHANCE_BASE +
                  (cnt - Heat::ITEM_SPILL_COMFORT - 1) * Heat::ITEM_SPILL_CHANCE_PER;
        if (pct > Heat::ITEM_SPILL_CHANCE_MAX) pct = Heat::ITEM_SPILL_CHANCE_MAX;
        if ((int)(model.prng(idx, 3) % 100u) < pct) amount = cnt / 2;
      }
    }
    if (amount > 0) {
      auto s = clk::now();
      int tgt = findSpillTarget(idx, model);
      spillMs += std::chrono::duration<double, std::milli>(clk::now() - s).count();
      // Nowhere to spill -> keep growing (a stalled stack must not freeze).
      if (tgt != -1) {
        spills.push_back({idx, tgt, item, amount});
        return;
      }
    }
    if (!model.itemGrow()) return;
    int nc = cnt + 1;
    if (nc > Heat::ITEM_COUNT_MAX) nc = Heat::ITEM_COUNT_MAX;
    muts.push_back({idx, CellType::ITEM, item, nc, CellEventType::GROW});
  }
}

void ItemCell::tick(int, GameModel &) {
  // Item tick is planned in parallel via collectTickPlan; serial part handled by GameModel.
}
