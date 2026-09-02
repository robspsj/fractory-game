#include "item_cell.hpp"
#include "game_model.hpp"
#include "tick_plan.hpp"
#include <algorithm>
#include <chrono>

int ItemCell::findSpillTarget(int idx, GameModel &model) {
  std::vector<int> candidates;

  int p = model._nodes[idx].parentId;
  if (p < 0) return -1;

  int parentFirst = model._nodes[p].content.data.grid.firstChild;
  int parentDim = model._nodes[p].content.data.grid.gridDimension;
  int localIdx = idx - parentFirst;
  int row = localIdx / parentDim;
  int col = localIdx % parentDim;

  auto addIfEmpty = [&](int ni) {
    if (ni >= 0 && ni < (int)model._nodes.size() &&
        model._nodes[ni].content.type == CellType::EMPTY) {
      candidates.push_back(ni);
    }
  };

  if (row > 0) addIfEmpty(parentFirst + (row - 1) * parentDim + col);
  if (row < parentDim - 1) addIfEmpty(parentFirst + (row + 1) * parentDim + col);
  if (col > 0) addIfEmpty(parentFirst + row * parentDim + (col - 1));
  if (col < parentDim - 1) addIfEmpty(parentFirst + row * parentDim + (col + 1));

  int gp = model._nodes[p].parentId;
  if (gp >= 0) {
    int gpFirst = model._nodes[gp].content.data.grid.firstChild;
    int gpDim = model._nodes[gp].content.data.grid.gridDimension;
    int parentLocal = p - gpFirst;
    int parentRow = parentLocal / gpDim;
    int parentCol = parentLocal % gpDim;

    auto checkGp = [&](int dr, int dc, int bRow, int bCol) {
      int gr = parentRow + dr, gc = parentCol + dc;
      if (gr < 0 || gr >= gpDim || gc < 0 || gc >= gpDim) return;
      int gi = gpFirst + gr * gpDim + gc;
      if (model._nodes[gi].content.type == CellType::EMPTY) {
        candidates.push_back(gi);
      } else if (model._nodes[gi].content.type == CellType::GRID) {
        int nf = model._nodes[gi].content.data.grid.firstChild;
        int nd = model._nodes[gi].content.data.grid.gridDimension;
        int r = std::min(bRow, nd - 1);
        int c = std::min(bCol, nd - 1);
        if (r >= 0 && c >= 0) addIfEmpty(nf + r * nd + c);
      }
    };

    if (row == 0) checkGp(-1, 0, parentDim - 1, col);
    if (row == parentDim - 1) checkGp(1, 0, 0, col);
    if (col == 0) checkGp(0, -1, row, parentDim - 1);
    if (col == parentDim - 1) checkGp(0, 1, row, 0);
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
  // Heat makes cells more reactive: hotter => more frequent ticks.
  // 0 heat: 1% (freqDenom 100), 255 heat: ~5% (freqDenom 20)
  int heat = cell.heat;
  int freqDenom = 100 - (heat * 80 / 255);
  if (freqDenom < 20) freqDenom = 20;
  if (freqDenom < 1) freqDenom = 1;
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
  int parent = cell.parentId;
  if (parent < 0) return;
  const Cell &parentCell = model._nodes[parent];
  if (parentCell.content.type != CellType::GRID) return;

  int dim = parentCell.content.data.grid.gridDimension;
  int first = parentCell.content.data.grid.firstChild;
  int localIdx = idx - first;
  int row = localIdx / dim;
  int col = localIdx % dim;

  struct { int dr, dc; } dirs[4] = {{-1,0},{1,0},{0,-1},{0,1}};

  for (auto &d : dirs) {
    int nr = row + d.dr;
    int nc = col + d.dc;
    if (nr < 0 || nr >= dim || nc < 0 || nc >= dim) continue;
    int nIdx = first + nr * dim + nc;
    const Cell &neighbor = model._nodes[nIdx];
    if (neighbor.content.type != CellType::ITEM) continue;

    const ReactionRule *rule =
        model._interactCfg.findReaction(cell.content.data.item.id, neighbor.content.data.item.id);
    if (rule) {
      cell.content.type = CellType::EMPTY;
      model._nodes[nIdx].content.type = CellType::EMPTY;
      cell.content.type = CellType::ITEM;
      cell.content.data.item.id = rule->result;
      cell.content.data.item.count = rule->resultCount;
      model.emitEvent(CellEventType::REACTION, idx, rule->result);
      model.addHeat(idx, 18);
      model.addHeat(nIdx, 12);
      return;
    }
  }
  cell.content.data.item.id = (cell.content.data.item.id + 1) % model._numItems;
  model.addHeat(idx, 6);
}

void ItemCell::tick(int, GameModel &) {
  // Item tick is planned in parallel via collectTickPlan; serial part handled by GameModel.
}
