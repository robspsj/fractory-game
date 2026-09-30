#include "game_model.hpp"
#include "empty_cell.hpp"
#include "grid_cell.hpp"
#include "item_cell.hpp"
#include "reserved_cell.hpp"
#include "station_cell.hpp"
#include "tick_plan.hpp"
#include "../heat_constants.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <unordered_set>

void GameModel::init(const Config &cfg) {
  _prngSeed = (cfg.seed != 0) ? (unsigned)cfg.seed : 0x9E3779B9u;
  _tickCounter = 0;
  _decayCur = _diffCur = 1;
  _numItems = (cfg.numItems > 0) ? cfg.numItems : GameModel::ELEMS;
  if (cfg.seed != 0)
    std::srand(cfg.seed);
  else
    std::srand((unsigned)std::time(nullptr));

  _nodes.clear();
  _type.clear();

  // Data-driven interaction config must be in place BEFORE the generator runs:
  // the random world scatters station footprints taken from the recipe table.
  initInteractConfig(_interactCfg, cfg.reactionsCsvPath, cfg.spawnCsvPath,
                     cfg.stationRecipesCsvPath, cfg.stationRecipesCsv,
                     cfg.reactionsCsv, cfg.spawnCsv, cfg.craftingCsvPath,
                     cfg.craftingCsv);
  // Recipe-book progress is per table row, so it is sized once the tables are
  // loaded (the book lists every entry and marks the performed ones).
  _book.resize((int)_interactCfg.crafting.size(),
               (int)_interactCfg.stationRecipes.size(),
               (int)_interactCfg.reactions.size());
  // Automatic item dynamics are opt-in feature flags (Config::itemGrow/…).
  _itemGrow = cfg.itemGrow;
  _itemShrink = cfg.itemShrink;
  _itemSplit = cfg.itemSplit;

  Cell root;
  _nodes.push_back(root);
  _type.push_back((uint8_t)CellType::EMPTY);

  if (cfg.emptyGrid) {
    initEmptyGrid(cfg.initialGridSize);
  } else {
    initRandomGrid(cfg);
  }
  rebuildAdjacency();

  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
  if (_events.size() < EVENT_CAP) _events.resize(EVENT_CAP);  // one-time alloc
  _evHead = _evTail = _eventDrops = 0;
}

void GameModel::initEmptyGrid(int size) {
  populateWithSubgrid(0, size);
}

// Random world generator. The (grid, item, empty) roll shares are the ones the
// single-roll generator used, so a given seed still builds the same world shape;
// what changed is that stations are planted for real:
//
//   pass 1  subgrids / items / empties  — one roll per cell, walking forward so
//           that appended subgrid children are visited as they are created
//   pass 2  stations — recipe footprints scattered over the cells pass 1 left
//           empty, anchored top-left like the hand-placed path
//
// Every pass only writes cells that are still EMPTY, and a successful station
// placement converts its whole footprint at once, so no pass can clobber a
// footprint claimed before it. (The single-roll version had both defects: its
// station branch was unreachable once the subgrid budget ran out, and it
// skipped a footprint by advancing `sizeR*sizeC` cells linearly — which only
// matches the row-major stride when sizeC equals the parent grid dimension, so
// the rest of the footprint was re-rolled into items/empties. It also ran
// before the recipe table was loaded, so it never placed anything at all.)
void GameModel::initRandomGrid(const Config &cfg) {
  const int gridChance = std::clamp(cfg.gridChance, 0, 100);
  // Clamped against gridChance so the roll shares below can never go negative
  // (a 100% station share plus the subgrid share is not a valid split).
  const int stationChance = std::clamp(cfg.stationChance, 0, 100 - gridChance);
  // Roll shares, unchanged from the single-roll generator: the station share is
  // still "the cells earmarked for a machine", the rest splits evenly between
  // items and empties.
  const int remaining = 100 - gridChance - stationChance;
  const int itemChance = remaining / 2;
  const int emptyChance = remaining - itemChance;

  populateWithSubgrid(0, GRID);
  int gridCount = 1;

  // Phases 1-2: children appended by populateWithSubgrid land after the current
  // index, so this same walk descends into nested subgrids as it reaches them.
  for (int i = 1; i < (int)_nodes.size(); i++) {
    if (typeOf(i) != CellType::EMPTY) continue;  // already a subgrid container
    const int roll = std::rand() % 100;
    if (roll < emptyChance) {
      // stays empty
    } else if (roll < emptyChance + itemChance) {
      setType(i, CellType::ITEM);
      _nodes[i].content.data.item.id = (uint16_t)(std::rand() % _numItems);
      _nodes[i].content.data.item.count = (uint16_t)(std::rand() % 4 + 1);
    } else if (roll < emptyChance + itemChance + stationChance) {
      // station share — left empty here, phase 3 anchors machines on it
    } else if (gridCount < cfg.gridLimit) {
      gridCount++;
      populateWithSubgrid(i, std::rand() % 6 + 2);
    } else {
      // Subgrid budget spent: what would have been a subgrid becomes an item,
      // as in the single-roll generator.
      setType(i, CellType::ITEM);
      _nodes[i].content.data.item.id = (uint16_t)(std::rand() % _numItems);
      _nodes[i].content.data.item.count = (uint16_t)(std::rand() % 4 + 1);
    }
  }

  // Pass 2: stations. One anchor roll per surviving empty cell; a successful
  // placement converts the whole footprint (STATION + RESERVED at once), so
  // footprint cells are never candidates themselves and two machines can never
  // overlap.
  if (stationChance > 0 && !_interactCfg.stationRecipes.empty()) {
    for (int i = 1; i < (int)_nodes.size(); i++) {
      if (typeOf(i) != CellType::EMPTY) continue;
      if (std::rand() % 100 >= stationChance) continue;
      tryPlaceRandomStation(i);
    }
  }

  // Seed visible heat for demo — see Heat::INIT_* in heat_constants.hpp
  for (int j = 1; j < (int)_nodes.size(); j++) {
    if (typeOf(j) == CellType::GRID) continue;
    if ((std::rand() % 100) < Heat::INIT_WARM_CHANCE_PCT)
      _nodes[j].heat = (uint8_t)(Heat::INIT_WARM_MIN + std::rand() % (Heat::INIT_WARM_MAX - Heat::INIT_WARM_MIN + 1));
  }
}

// Plant a station at `anchorIndex`, which must be the footprint's top-left cell
// — that is what StationCell's layout math (and the hand-placed path) assumes.
// Recipes are tried in a random rotation so a small machine still lands where
// the big one has no room. Returns false when nothing fits.
bool GameModel::tryPlaceRandomStation(int anchorIndex) {
  if (anchorIndex <= 0 || anchorIndex >= (int)_nodes.size()) return false;
  if (_interactCfg.stationRecipes.empty()) return false;
  if (typeOf(anchorIndex) != CellType::EMPTY) return false;

  Cell &anchor = _nodes[anchorIndex];
  const int parentIndex = anchor.parentId;
  if (parentIndex < 0 || typeOf(parentIndex) != CellType::GRID) return false;

  const Cell &parent = _nodes[parentIndex];
  const int dim = parent.content.data.grid.gridDimension;
  const int first = parent.content.data.grid.firstChild;
  const int local = anchorIndex - first;
  const int startRow = local / dim;
  const int startCol = local % dim;

  const int recipeCount = (int)_interactCfg.stationRecipes.size();
  const int offset = std::rand() % recipeCount;
  for (int k = 0; k < recipeCount; k++) {
    const StationRecipe &recipe = _interactCfg.stationRecipes[(offset + k) % recipeCount];
    if (recipe.sizeR <= 0 || recipe.sizeC <= 0) continue;
    if (startRow + recipe.sizeR > dim) continue;
    if (startCol + recipe.sizeC > dim) continue;

    bool free = true;
    for (int r = 0; r < recipe.sizeR && free; r++) {
      for (int c = 0; c < recipe.sizeC; c++) {
        const int idx = first + (startRow + r) * dim + (startCol + c);
        if (idx != anchorIndex && typeOf(idx) != CellType::EMPTY) { free = false; break; }
      }
    }
    if (!free) continue;

    setType(anchorIndex, CellType::STATION);
    anchor.content.data.station = {0, (uint16_t)recipe.id, (uint8_t)recipe.sizeR, (uint8_t)recipe.sizeC};

    for (int r = 0; r < recipe.sizeR; r++) {
      for (int c = 0; c < recipe.sizeC; c++) {
        const int idx = first + (startRow + r) * dim + (startCol + c);
        if (idx == anchorIndex) continue;
        setType(idx, CellType::RESERVED);
        _nodes[idx].content.data.reserved = {
          (uint16_t)(idx - anchorIndex),
          recipe.layout[r * recipe.sizeC + c],
          NO_ITEM, 0
        };
      }
    }
    return true;
  }
  return false;
}

void GameModel::populateWithSubgrid(int cellIndex, int size) {
  GridCell::populateWithSubgrid(cellIndex, size, _nodes, _type);
}

// Full rebuild of the adjacency table. Only topology changes (init,
// setFullState) need it — drop/station/heat/spill mutate content in place,
// never geometry, so indices and rects stay valid.
void GameModel::rebuildAdjacency() {
  assert(_type.size() == _nodes.size());
  _adj.assign(_nodes.size(), Adjacency{});
  if (_nodes.empty() || typeOf(0) != CellType::GRID) return;
  const double R = _nodes[0].content.data.grid.gridDimension;  // root spans [0,R)²

  // Deepest cell containing world point (px,py), or -1. Iterative descend.
  auto leafAt = [&](double px, double py) -> int {
    int g = 0;
    double x = 0.0, y = 0.0, w = R, h = R;
    for (;;) {
      if (!(px >= x && px < x + w && py >= y && py < y + h)) return -1;
      const Cell &gn = _nodes[g];
      if (_type[g] != (uint8_t)CellType::GRID) return g;
      const int d = gn.content.data.grid.gridDimension;
      const int f = gn.content.data.grid.firstChild;
      const double cw = w / d, ch = h / d;
      int c = (int)((px - x) / cw);
      int r = (int)((py - y) / ch);
      c = std::clamp(c, 0, d - 1);  // guard fp noise at edges
      r = std::clamp(r, 0, d - 1);
      g = f + r * d + c;
      x += c * cw; y += r * ch; w = cw; h = ch;
    }
  };

  // Fast path: same-grid sibling that is itself a leaf (the common case)
  // avoids a descend; anything crossing hierarchy or landing on a GRID
  // container falls through to leafAt().
  auto side = [&](int leafCell, int dir, double px, double py) -> int {
    const int g = _nodes[leafCell].parentId;
    if (g < 0 || _type[g] != (uint8_t)CellType::GRID) return leafAt(px, py);
    const Cell &gn = _nodes[g];
    const int d = gn.content.data.grid.gridDimension;
    const int f = gn.content.data.grid.firstChild;
    const int local = leafCell - f;
    const int r = local / d, c = local % d;
    int sib = -1;
    switch (dir) {
      case ADJ_UP:    if (r > 0)     sib = f + (r - 1) * d + c; break;
      case ADJ_DOWN:  if (r < d - 1) sib = f + (r + 1) * d + c; break;
      case ADJ_LEFT:  if (c > 0)     sib = f + r * d + (c - 1); break;
      case ADJ_RIGHT: if (c < d - 1) sib = f + r * d + (c + 1); break;
    }
    if (sib >= 0 && _type[sib] != (uint8_t)CellType::GRID) return sib;
    return leafAt(px, py);
  };

  struct Frame { int cell; double x, y, w, h; };  // cell = its GRID node
  std::vector<Frame> stack{{0, 0.0, 0.0, R, R}};
  while (!stack.empty()) {
    Frame fr = stack.back();
    stack.pop_back();
    const Cell &gn = _nodes[fr.cell];
    if (_type[fr.cell] != (uint8_t)CellType::GRID) continue;
    const int d = gn.content.data.grid.gridDimension;
    const int f = gn.content.data.grid.firstChild;
    const double cw = fr.w / d, ch = fr.h / d;
    const double eps = 1e-3 * std::min(cw, ch);
    for (int r = 0; r < d; r++) {
      for (int c = 0; c < d; c++) {
        const int child = f + r * d + c;
        const double x = fr.x + c * cw, y = fr.y + r * ch;
        if (_type[child] == (uint8_t)CellType::GRID) {
          stack.push_back({child, x, y, cw, ch});
          continue;
        }
        const double mx = x + cw * 0.5, my = y + ch * 0.5;
        Adjacency &a = _adj[child];
        a[ADJ_UP]    = side(child, ADJ_UP,    mx, y - eps);
        a[ADJ_DOWN]  = side(child, ADJ_DOWN,  mx, y + ch + eps);
        a[ADJ_LEFT]  = side(child, ADJ_LEFT,  x - eps, my);
        a[ADJ_RIGHT] = side(child, ADJ_RIGHT, x + cw + eps, my);
        for (int32_t &e : a.n) if (e == child) e = -1;  // fp safety
      }
    }
  }
}

void GameModel::pickUp(int nodeIndex, int amount) {
  if (nodeIndex <= 0 || nodeIndex >= (int)_nodes.size())
    return;
  if (typeOf(nodeIndex) != CellType::ITEM)
    return;

  _dragSrcIndex = nodeIndex;
  _dragItemId = _nodes[nodeIndex].content.data.item.id;
  _dragAmount = std::min(amount, (int)_nodes[nodeIndex].content.data.item.count);
  _dragHeat = _nodes[nodeIndex].heat;
  int left = (int)_nodes[nodeIndex].content.data.item.count - _dragAmount;
  _nodes[nodeIndex].content.data.item.count = (uint16_t)(left > 0 ? left : 0);
  if (left <= 0) {
    setType(nodeIndex, CellType::EMPTY);
  }
}

void GameModel::drop(int nodeIndex) {
  if (!hasDrag())
    return;

  CellContent dragCell;
  dragCell.data.item.id = _dragItemId;
  dragCell.data.item.count = _dragAmount;

  if (nodeIndex > 0 && nodeIndex < (int)_nodes.size()) {
    const CellType tgtType = typeOf(nodeIndex);
    if (tgtType == CellType::ITEM &&
        _nodes[nodeIndex].content.data.item.id == _dragItemId) {
      int nc = (int)_nodes[nodeIndex].content.data.item.count + _dragAmount;
      _nodes[nodeIndex].content.data.item.count = (uint16_t)(nc > 65535 ? 65535 : nc);
      // keep heat with cell, not item — no heat change on merge
      emitEvent(CellEventType::MERGE, nodeIndex, _dragItemId);
      _book.mark(BookCategory::Stacking, (int)BookPhysics::Merge);
    } else if (tgtType == CellType::EMPTY) {
      putContent(nodeIndex, CellType::ITEM, dragCell);
      // keep target's heat (cell-local), don't transfer item heat
    } else if (tgtType == CellType::ITEM) {
      CellContent temp = _nodes[nodeIndex].content;
      putContent(nodeIndex, CellType::ITEM, dragCell);
      putContent(_dragSrcIndex, CellType::ITEM, temp);
      // heats stay with cells
      _dragSrcIndex = -1;
      _dragAmount = 0;
      _dragItemId = -1;
      _dragHeat = 0;
      return;
    } else {
      cancelDrag();
      return;
    }
  } else {
    putContent(_dragSrcIndex, CellType::ITEM, dragCell);
  }

  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
  _dragHeat = 0;
}

void GameModel::cancelDrag() {
  if (!hasDrag())
    return;
  CellContent c;
  c.data.item.id = _dragItemId;
  c.data.item.count = _dragAmount;
  putContent(_dragSrcIndex, CellType::ITEM, c);
  // heat stays with cell, dragged heat is just visual; no need to restore
  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
  _dragHeat = 0;
}

void GameModel::drainEvents(std::vector<CellEvent> &out) {
  // Copy path kept for the test/bench harnesses; the game uses consumeEvents()
  // (in-place, no copy).
  for (; _evHead != _evTail; ++_evHead)
    out.push_back(_events[_evHead & EVENT_MASK]);
}

void GameModel::interact(int idx) {
  if (idx <= 0 || idx >= (int)_nodes.size()) return;

  CellType t = typeOf(idx);
  switch (t) {
    case CellType::EMPTY: EmptyCell::interact(idx, *this); break;
    case CellType::ITEM: ItemCell::interact(idx, *this); break;
    case CellType::GRID: GridCell::interact(idx, *this); break;
    case CellType::STATION: StationCell::interact(idx, *this); break;
    case CellType::RESERVED: ReservedCell::interact(idx, *this); break;
  }
}

int GameModel::findSpillTarget(int idx) {
  return ItemCell::findSpillTarget(idx, *this);
}

uint32_t GameModel::prng(int idx, int salt) const {
  if (_rngOverride >= 0)
    return (uint32_t)_rngOverride;
  uint32_t x = (uint32_t)idx ^ (uint32_t)salt * 0x9E3779B1u;
  x ^= _prngSeed;
  x ^= (uint32_t)_tickCounter * 0x85EBCA6Du;
  x += 0x9E3779B9u;
  x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
  x = (x ^ (x >> 13)) * 0xC2B2AE35u;
  x ^= (x >> 16);
  return x;
}

bool GameModel::quantumFluctuation(int idx) {
  if (idx <= 0 || idx >= (int)_nodes.size()) return false;
  if (typeOf(idx) != CellType::EMPTY) return false;   // vacuum only
  if (_nodes[idx].heat < Heat::QUANTUM_MIN_HEAT) return false;
  if ((int)(prng(idx, Heat::QUANTUM_PRNG_SALT) % 100u) >= Heat::QUANTUM_CHANCE_PCT)
    return false;
  // The fluctuation wins: a particle pops into being and the cell's energy
  // is absorbed — it freezes to heat 0, which is both the birth signature
  // and the cooldown (the hold must pump ~2s back up to the threshold).
  CellContent born;
  born.data.item.id =
      (uint16_t)(prng(idx, Heat::QUANTUM_PRNG_SALT + 1) % (uint32_t)_numItems);
  born.data.item.count = 1;
  putContent(idx, CellType::ITEM, born);
  _nodes[idx].heat = 0;
  emitEvent(CellEventType::QUANTUM_SPAWN, idx, born.data.item.id);
  _book.mark(BookCategory::Quantum, (int)BookPhysics::Quantum);
  return true;
}

int GameModel::stationRecipeIndexOf(int recipeId) const {
  const auto &v = _interactCfg.stationRecipes;
  for (int i = 0; i < (int)v.size(); i++)
    if (v[i].id == recipeId) return i;
  return -1;
}

int GameModel::cellAtOffset(int idx, int dr, int dc) const {
  if (idx <= 0 || idx >= (int)_nodes.size()) return -1;
  const int parent = _nodes[idx].parentId;
  if (parent < 0 || typeOf(parent) != CellType::GRID) return -1;
  const int dim = _nodes[parent].content.data.grid.gridDimension;
  const int first = _nodes[parent].content.data.grid.firstChild;
  const int off = idx - first;
  if (off < 0 || off >= dim * dim) return -1;
  const int r = off / dim, c = off % dim;
  const int nr = r + dr, nc = c + dc;
  if (nr < 0 || nr >= dim || nc < 0 || nc >= dim) return -1;
  return first + nr * dim + nc;
}

bool GameModel::tryCraftWithDrag(int anchorIdx) {
  if (!hasDrag() || _dragItemId != CRAFT_TOOL_ITEM) return false;
  if (anchorIdx <= 0 || anchorIdx >= (int)_nodes.size()) return false;
  if (typeOf(anchorIdx) != CellType::ITEM) return false;

  for (int ri = 0; ri < (int)_interactCfg.crafting.size(); ri++) {
    const CraftingRule &rule = _interactCfg.crafting[ri];
    // Slide the recipe so the clicked cell can sit anywhere inside its bounding
    // box: origin = anchor - (dr,dc). First match wins (the rules are tried in
    // file order, so specific shapes should be authored before general ones).
    for (int dr = rule.minDr; dr <= rule.maxDr; dr++) {
      for (int dc = rule.minDc; dc <= rule.maxDc; dc++) {
        const int origin = cellAtOffset(anchorIdx, -dr, -dc);
        if (origin < 0) continue;
        // The clicked cell must itself be one of the pattern's cells.
        bool anchorInPattern = false;
        for (const CraftEntry &e : rule.entries)
          if (cellAtOffset(origin, e.dr, e.dc) == anchorIdx) { anchorInPattern = true; break; }
        if (!anchorInPattern) continue;
        // Every entry must be a matching ITEM cell.
        bool ok = true;
        for (const CraftEntry &e : rule.entries) {
          const int c = cellAtOffset(origin, e.dr, e.dc);
          if (c < 0 || typeOf(c) != CellType::ITEM ||
              _nodes[c].content.data.item.id != (uint16_t)e.itemId) {
            ok = false;
            break;
          }
        }
        if (!ok) continue;
        // The anchor loses one unit like every other pattern cell, so after the
        // craft it is either free (result placed) or holds a leftover of the
        // result item (result merged in). Anything else would silently destroy
        // items, so the craft is refused instead.
        const int left = (int)_nodes[anchorIdx].content.data.item.count - 1;
        if (left > 0 &&
            _nodes[anchorIdx].content.data.item.id != (uint16_t)rule.result)
          continue;
        // Commit: one unit per pattern cell, then the result on the anchor.
        for (const CraftEntry &e : rule.entries) {
          const int c = cellAtOffset(origin, e.dr, e.dc);
          const int n = (int)_nodes[c].content.data.item.count - 1;
          if (n <= 0)
            setType(c, CellType::EMPTY);
          else
            _nodes[c].content.data.item.count = (uint16_t)n;
        }
        CellContent res;
        res.data.item.id = (uint16_t)rule.result;
        res.data.item.count =
            (uint16_t)(left > 0 ? left + rule.resultCount : rule.resultCount);
        putContent(anchorIdx, CellType::ITEM, res);
        addHeat(anchorIdx, Heat::CRAFT_HEAT);
        emitEvent(CellEventType::CRAFT, anchorIdx, rule.result);
        _book.mark(BookCategory::Recipes, ri);
        // The crafting element is spent one unit per craft.
        if (--_dragAmount <= 0) {
          _dragSrcIndex = -1;
          _dragAmount = 0;
          _dragItemId = -1;
          _dragHeat = 0;
        }
        return true;
      }
    }
  }
  return false;
}

void GameModel::tick() {
  using clk = std::chrono::steady_clock;
  _tprof = TickProfile{};
  _tprof.nodes = (int)_nodes.size();
  ++_tickCounter;
  auto wall = clk::now();

  const int n = static_cast<int>(_nodes.size());
  int nThreads = std::clamp((int)std::thread::hardware_concurrency(), 1, 16);
  if (const char *tg = std::getenv("FRACTORY_TICK_THREADS")) {
    int v = std::atoi(tg);
    if (v >= 1) nThreads = std::min(v, 64);
  }
  if (n < 16384 && nThreads > 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, n));

  if (_tickBufs.size() < (size_t)nThreads) _tickBufs.resize((size_t)nThreads);
  for (int t = 0; t < nThreads; t++) {
    ThreadBuf &b = _tickBufs[t];
    b.muts.clear(); b.spills.clear(); b.stations.clear();
    b.items = 0; b.spillMs = 0.0;
  }
  auto &bufs = _tickBufs;
  const int64_t bigN = n;
  auto worker = [&](int tid) {
    const int b = (int)((int64_t)tid * bigN / nThreads);
    const int e = (int)((int64_t)(tid + 1) * bigN / nThreads);
    ThreadBuf &buf = bufs[tid];
    for (int i = b; i < e; i++) {
      // Prescan the 1-byte type map first: cells that cannot act (EMPTY,
      // GRID) are skipped without touching their Cell cache lines.
      const uint8_t t = _type[i];
      if (t == (uint8_t)CellType::ITEM) {
        ItemCell::collectTickPlan(i, _nodes[i], *this, buf.muts, buf.spills, buf.spillMs, buf.items);
      } else if (t == (uint8_t)CellType::STATION) {
        StationCell::collectTickPlan(i, *this, buf.stations);
      }
    }
  };

  if (nThreads <= 1) {
    worker(0);
  } else {
    std::vector<std::thread> threads;
    threads.reserve(nThreads);
    for (int t = 0; t < nThreads; t++) threads.emplace_back(worker, t);
    for (auto &th : threads) th.join();
  }
  _tprof.planMs = std::chrono::duration<double, std::milli>(clk::now() - wall).count();

  for (int t = 0; t < nThreads; t++) {
    const ThreadBuf &buf = bufs[t];
    _tprof.items   += (int)buf.items;
    _tprof.spills  += (int)buf.spills.size();
    _tprof.spillMs += buf.spillMs;
  }

  auto tApply = clk::now();
  // First-wins spill claims; persistent set so no per-tick bucket allocation
  // (clear() keeps the bucket array).
  _spillClaimed.clear();
  for (int t = 0; t < nThreads; t++) {
    ThreadBuf &buf = bufs[t];
    for (const PlMut &m : buf.muts) {
      Cell &cl = _nodes[m.idx];
      if (m.ev == CellEventType::DESTROY)
        setType(m.idx, CellType::EMPTY);
      else
        cl.content.data.item.count = m.count;
      emitEvent(m.ev, m.idx, m.item);
      // Recipe book: mark the stacking transformations as performed.
      if (m.ev == CellEventType::GROW)
        _book.mark(BookCategory::Stacking, (int)BookPhysics::Grow);
      else if (m.ev == CellEventType::SHRINK)
        _book.mark(BookCategory::Stacking, (int)BookPhysics::Shrink);
      else if (m.ev == CellEventType::DESTROY)
        _book.mark(BookCategory::Stacking, (int)BookPhysics::Destroy);
      // heat feedback from item activity — see Heat::ITEM_MUT_* / SPILL_*
      if (typeOf(m.idx) != CellType::GRID) {
        int dh = Heat::ITEM_MUT_BASE + ((Heat::HEAT_MAX - cl.heat) >> Heat::ITEM_MUT_SCALE_SHIFT);
        int nh = (int)cl.heat + dh;
        if (nh > 255) nh = 255;
        cl.heat = (uint8_t)nh;
      }
    }
    for (const PlSpill &sp : buf.spills) {
      if (!_spillClaimed.insert(sp.tgt).second) continue;
      // Proportional split: the source keeps what it did not send (sp.amount
      // is half the planned stack), so big piles drain without loss.
      Cell &srcCell = _nodes[sp.src];
      int left = (int)srcCell.content.data.item.count - sp.amount;
      if (left < 0) left = 0;
      srcCell.content.data.item.count = (uint16_t)left;
      if (left == 0) setType(sp.src, CellType::EMPTY);
      setType(sp.tgt, CellType::ITEM);
      _nodes[sp.tgt].content.data.item.id  = sp.item;
      _nodes[sp.tgt].content.data.item.count = (uint16_t)sp.amount;
      emitEvent(CellEventType::SPILL, sp.src, sp.item);
      _book.mark(BookCategory::Stacking, (int)BookPhysics::Split);
      addHeat(sp.src, Heat::SPILL_HEAT_SRC);
      addHeat(sp.tgt, Heat::SPILL_HEAT_TGT);
    }
  }
  _tprof.applyMs = std::chrono::duration<double, std::milli>(clk::now() - tApply).count();

  auto tSt = clk::now();
  for (int t = 0; t < nThreads; t++)
    for (int si : bufs[t].stations) tickStation(si);
  _tprof.stationMs = std::chrono::duration<double, std::milli>(clk::now() - tSt).count();

  // Heat decay & diffusion — see Heat::DECAY_* / DIFFUSION_* in heat_constants.hpp
  // Amortized: a slice of the array is processed every tick (per-cell period
  // unchanged), instead of the whole array every 32nd/64th tick.
  auto tDecay = clk::now();
  {
    int begin = (_decayCur > n || _decayCur < 1) ? 1 : _decayCur;
    int end = begin + n / (Heat::DECAY_PERIOD_MASK + 1) + 1;
    if (end > n) end = n;
    for (int i = begin; i < end; i++) {
      if (_type[i] == (uint8_t)CellType::GRID) continue;
      Cell &c = _nodes[i];
      if (c.heat == 0) continue;
      // Quantum fluctuation piggybacks on this pass (~every 32 ticks per cell
      // — the natural rate knob): a superheated EMPTY vacuum may condense an
      // item, which zeroes this cell's heat, so skip the local decay below.
      if (typeOf(i) == CellType::EMPTY &&
          c.heat >= Heat::QUANTUM_MIN_HEAT && quantumFluctuation(i))
        continue;
      int d = (c.heat >> Heat::DECAY_PROPORTIONAL_SHIFT) + 1;  // proportional loss
      c.heat = (uint8_t)(c.heat > d ? c.heat - d : 0);
    }
    _decayCur = (end >= n) ? 1 : end;
  }
  _tprof.decayMs = std::chrono::duration<double, std::milli>(clk::now() - tDecay).count();
  auto tDiff = clk::now();
  {
    int begin = (_diffCur > n || _diffCur < 1) ? 1 : _diffCur;
    int end = begin + n / (Heat::DIFFUSION_PERIOD_MASK + 1) + 1;
    if (end > n) end = n;
    for (int i = begin; i < end; i++) {
      Cell &c = _nodes[i];
      if (_type[i] == (uint8_t)CellType::GRID) continue;
      if (c.heat <= 1) continue;
      if (( (uint32_t)i * 2654435761u + (uint32_t)_tickCounter) & Heat::DIFFUSION_SPARSE_MASK) continue;
      // Spatial diffusion: pass heat to one random world-space neighbour
      // (precomputed adjacency, possibly across subgrid boundaries).
      const Adjacency &a = _adj[i];
      uint32_t r = (uint32_t)i * 1103515245u + (uint32_t)_tickCounter * 134775813u;
      int tgt = a.n[(int)(r & 3u)];
      if (tgt < 0 || tgt >= n) continue;
      if (_type[tgt] == (uint8_t)CellType::GRID) continue;
      c.heat--;
      uint8_t &th = _nodes[tgt].heat;
      if (th < 255) th++;
    }
    _diffCur = (end >= n) ? 1 : end;
  }
  _tprof.diffuseMs = std::chrono::duration<double, std::milli>(clk::now() - tDiff).count();

  _tprof.sweepMs =
      std::chrono::duration<double, std::milli>(clk::now() - wall).count();

  // Running totals across all ticks (for --tick phase reports).
  _taccum.sweepMs   += _tprof.sweepMs;
  _taccum.planMs    += _tprof.planMs;
  _taccum.applyMs   += _tprof.applyMs;
  _taccum.stationMs += _tprof.stationMs;
  _taccum.decayMs   += _tprof.decayMs;
  _taccum.diffuseMs += _tprof.diffuseMs;
  _taccum.spillMs   += _tprof.spillMs;
  _taccum.emitMs    += _tprof.emitMs;
  _taccum.events    += _tprof.events;
  _taccum.spills    += _tprof.spills;
  _taccum.items     += _tprof.items;
  _taccum.nodes      = n;

#ifdef FRACTORY_TYPECHECK
  // Dev build (-DFRACTORY_TYPECHECK=1): type lives ONLY in _type now, so a
  // desync is unrepresentable — the residual risk is length skew from a node
  // push that forgot to mirror into _type (the view would read garbage).
  if ((int)_type.size() != n) {
    fprintf(stderr, "TYPEMAP LENGTH SKEW: nodes=%d types=%zu\n", n, _type.size());
    abort();
  }
#endif
}

void GameModel::tickEmpty(int idx) {
  EmptyCell::tick(idx, *this);
}

void GameModel::tickGrid(int idx) {
  GridCell::tick(idx, *this);
}

void GameModel::tickStation(int idx) {
  StationCell::tick(idx, *this);
}

void GameModel::tickItem(int idx) {
  ItemCell::tick(idx, *this);
}

void GameModel::tryStartStation(int anchorIdx) {
  StationCell::tryStart(anchorIdx, *this);
}

void GameModel::setFullState(int *inData) {
  int first = _nodes[0].content.data.grid.firstChild;
  int n = _nodes[0].content.data.grid.gridDimension * _nodes[0].content.data.grid.gridDimension;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    int id = inData[i * 2];
    int count = inData[i * 2 + 1];
    if (id == -2) {
      if (typeOf(idx) != CellType::GRID) {
        setType(idx, CellType::GRID);
        _nodes[idx].content.data.grid.firstChild = (int)_nodes.size();
        _nodes[idx].content.data.grid.gridDimension = 3;
        _nodes[idx].content.data.grid.variation = 3;
        for (int j = 0; j < 9; j++) {
          Cell child;
          child.parentId = idx;
          _nodes.push_back(child);
          _type.push_back((uint8_t)CellType::EMPTY);
        }
      }
    } else if (id == -1) {
      setType(idx, CellType::EMPTY);
    } else {
      setType(idx, CellType::ITEM);
      _nodes[idx].content.data.item.id = id;
      _nodes[idx].content.data.item.count = count;
    }
  }
  rebuildAdjacency();
}

void GameModel::getFullState(int *outData) const {
  int first = _nodes[0].content.data.grid.firstChild;
  int n = _nodes[0].content.data.grid.gridDimension * _nodes[0].content.data.grid.gridDimension;
  int subgridIdx = 0;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    if (typeOf(idx) == CellType::ITEM) {
      outData[i * 2] = _nodes[idx].content.data.item.id;
      outData[i * 2 + 1] = _nodes[idx].content.data.item.count;
    } else if (typeOf(idx) == CellType::GRID) {
      outData[i * 2] = -2;
      outData[i * 2 + 1] = subgridIdx++;
    } else {
      outData[i * 2] = -1;
      outData[i * 2 + 1] = 0;
    }
  }
}

int GameModel::getSubgridState(int subgridSeqIndex, int *outData,
                               int &outSize) const {
  int first = _nodes[0].content.data.grid.firstChild;
  int n = _nodes[0].content.data.grid.gridDimension * _nodes[0].content.data.grid.gridDimension;
  int subgridIdx = 0;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    if (typeOf(idx) == CellType::GRID) {
      if (subgridIdx == subgridSeqIndex) {
        int childFirst = _nodes[idx].content.data.grid.firstChild;
        int childSize = _nodes[idx].content.data.grid.gridDimension;
        outSize = childSize;
        int childCount = childSize * childSize;
        for (int ci = 0; ci < childCount; ci++) {
          int cidx = childFirst + ci;
          if (typeOf(cidx) == CellType::ITEM) {
            outData[ci * 2] = _nodes[cidx].content.data.item.id;
            outData[ci * 2 + 1] = _nodes[cidx].content.data.item.count;
          } else if (typeOf(cidx) == CellType::GRID) {
            outData[ci * 2] = -2;
            outData[ci * 2 + 1] = -1;
          } else {
            outData[ci * 2] = -1;
            outData[ci * 2 + 1] = 0;
          }
        }
        return childFirst;
      }
      subgridIdx++;
    }
  }
  outSize = 0;
  return 0;
}

void GameModel::getDragState(int &outId, int &outCount) {
  outId = _dragItemId;
  outCount = _dragAmount;
}

bool GameModel::placeStation(int gridIndex, int recipeId) {
  return StationCell::place(gridIndex, recipeId, *this);
}

void GameModel::removeStation(int anchorIndex) {
  StationCell::remove(anchorIndex, *this);
}

uint8_t GameModel::heatAt(int idx) const {
  if (idx < 0 || idx >= (int)_nodes.size()) return 0;
  if (typeOf(idx) == CellType::GRID) return 0;
  return _nodes[idx].heat;
}
void GameModel::setHeat(int idx, uint8_t h) {
  if (idx < 0 || idx >= (int)_nodes.size()) return;
  if (typeOf(idx) == CellType::GRID) return;
  _nodes[idx].heat = h;
}
void GameModel::addHeat(int idx, int delta) {
  if (idx < 0 || idx >= (int)_nodes.size()) return;
  Cell &c = _nodes[idx];
  if (typeOf(idx) == CellType::GRID) return;
  int nh = (int)c.heat + delta;
  if (nh < 0) nh = 0;
  if (nh > 255) nh = 255;
  c.heat = (uint8_t)nh;
}
void GameModel::setHeatBulk(const std::vector<std::pair<int,uint8_t>> &pairs) {
  for (auto &p : pairs) setHeat(p.first, p.second);
}
