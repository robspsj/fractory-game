#include "game_model.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <unordered_set>

void GameModel::init(const Config &cfg) {
  _prngSeed = (cfg.seed != 0) ? (unsigned)cfg.seed : 0x9E3779B9u;
  _tickCounter = 0;
  if (cfg.seed != 0)
    std::srand(cfg.seed);
  else
    std::srand((unsigned)std::time(nullptr));

  _nodes.clear();

  Cell root;
  _nodes.push_back(root);

  if (cfg.emptyGrid) {
    initEmptyGrid(cfg.initialGridSize);
  } else {
    initRandomGrid(cfg);
  }

  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
  _events.clear();

  initInteractConfig(_interactCfg, cfg.reactionsCsvPath, cfg.spawnCsvPath,
                     cfg.stationRecipesCsvPath);
}

void GameModel::initEmptyGrid(int size) {
  populateWithSubgrid(0, size);
}

void GameModel::initRandomGrid(const Config &cfg) {
  int gridChance = cfg.gridChance;
  int stationChance = cfg.stationChance;

  populateWithSubgrid(0, GRID);
  int gridCount = 1;

  int remaining = 100 - gridChance - stationChance;
  int itemChance = remaining / 2;
  int emptyChance = remaining - itemChance;

  int i = 1;
  while (i < (int)_nodes.size()) {
    auto &cell = _nodes[i];

    int randVal = std::rand() % 100;
    if (randVal < emptyChance) {
      cell.content.type = CellType::EMPTY;
    } else if (randVal < emptyChance + itemChance || gridCount >= cfg.gridLimit) {
      cell.content.type = CellType::ITEM;
      cell.content.data.item.id = std::rand() % ELEMS;
      cell.content.data.item.count = std::rand() % 4 + 1;
    } else if (randVal < emptyChance + itemChance + stationChance) {
      // Try to place a station
      if (tryPlaceRandomStation(i)) {
        // Station placed, skip its cells
        const StationData &st = _nodes[i].content.data.station;
        i += st.sizeR * st.sizeC - 1; // -1 because loop increments
      } else {
        // Couldn't place, fall back to empty
        cell.content.type = CellType::EMPTY;
      }
    } else {
      cell.content.type = CellType::GRID;
      gridCount++;
      int subSize = std::rand() % 6 + 2; // sub-grid dimension: 2..7
      populateWithSubgrid(i, subSize);
    }
    i++;
  }
}

bool GameModel::tryPlaceRandomStation(int gridIndex) {
  if (gridIndex <= 0 || gridIndex >= (int)_nodes.size()) return false;
  if (_interactCfg.stationRecipes.empty()) return false;

  Cell &cell = _nodes[gridIndex];
  if (cell.parentId < 0) return false;
  if (cell.content.type != CellType::EMPTY) return false;

  const Cell &parentCell = _nodes[cell.parentId];
  if (parentCell.content.type != CellType::GRID) return false;

  int parentDim = parentCell.content.data.grid.gridDimension;
  int parentFirst = parentCell.content.data.grid.firstChild;
  int localIdx = gridIndex - parentFirst;
  int startRow = localIdx / parentDim;
  int startCol = localIdx % parentDim;

  // Pick a random recipe
  int recipeIdx = std::rand() % (int)_interactCfg.stationRecipes.size();
  const StationRecipe &recipe = _interactCfg.stationRecipes[recipeIdx];

  // Check if it fits
  if (startRow + recipe.sizeR > parentDim) return false;
  if (startCol + recipe.sizeC > parentDim) return false;

  // Check all cells are empty
  for (int r = 0; r < recipe.sizeR; r++) {
    for (int c = 0; c < recipe.sizeC; c++) {
      int idx = parentFirst + (startRow + r) * parentDim + (startCol + c);
      if (_nodes[idx].content.type != CellType::EMPTY) return false;
    }
  }

  // Place anchor
  cell.content.type = CellType::STATION;
  cell.content.byteSize = calcByteSize(CellType::STATION);
  cell.content.data.station = {recipe.id, 0, recipe.sizeR, recipe.sizeC};

  // Place reserved cells
  for (int r = 0; r < recipe.sizeR; r++) {
    for (int c = 0; c < recipe.sizeC; c++) {
      int row = startRow + r;
      int col = startCol + c;
      int idx = parentFirst + row * parentDim + col;
      if (idx == gridIndex) continue;
      int layoutIdx = r * recipe.sizeC + c;
      _nodes[idx].content.type = CellType::RESERVED;
      _nodes[idx].content.byteSize = calcByteSize(CellType::RESERVED);
      _nodes[idx].content.data.reserved = {
        gridIndex,
        recipe.layout[layoutIdx],
        -1, 0
      };
    }
  }

  return true;
}

void GameModel::populateWithSubgrid(int cellIndex, int size) {
  auto &cell = _nodes[cellIndex];
  cell.content.type = CellType::GRID;
  cell.content.data.grid.gridDimension = size;
  cell.content.data.grid.firstChild = (int)_nodes.size();
  cell.content.data.grid.variation = size;

  for (int j = 0; j < size * size; j++) {
    Cell child;
    child.parentId = cellIndex;
    child.content.type = CellType::EMPTY;
    _nodes.push_back(child);
  }
}

void GameModel::pickUp(int nodeIndex, int amount) {
  if (nodeIndex <= 0 || nodeIndex >= (int)_nodes.size())
    return;
  if (_nodes[nodeIndex].content.type != CellType::ITEM)
    return;

  _dragSrcIndex = nodeIndex;
  _dragItemId = _nodes[nodeIndex].content.data.item.id;
  _dragAmount = std::min(amount, _nodes[nodeIndex].content.data.item.count);
  _nodes[nodeIndex].content.data.item.count -= _dragAmount;
  if (_nodes[nodeIndex].content.data.item.count <= 0) {
    _nodes[nodeIndex].content.type = CellType::EMPTY;
  }
}

void GameModel::drop(int nodeIndex) {
  if (!hasDrag())
    return;

  CellContent dragCell;
  dragCell.type = CellType::ITEM;
  dragCell.data.item.id = _dragItemId;
  dragCell.data.item.count = _dragAmount;

  if (nodeIndex > 0 && nodeIndex < (int)_nodes.size()) {
    if (_nodes[nodeIndex].content.type == CellType::ITEM &&
        _nodes[nodeIndex].content.data.item.id == _dragItemId) {
      _nodes[nodeIndex].content.data.item.count += _dragAmount;
    } else if (_nodes[nodeIndex].content.type == CellType::EMPTY) {
      _nodes[nodeIndex].content = dragCell;
    } else if (_nodes[nodeIndex].content.type == CellType::ITEM) {
      CellContent temp = _nodes[nodeIndex].content;
      _nodes[nodeIndex].content = dragCell;
      _nodes[_dragSrcIndex].content = temp;
      _dragSrcIndex = -1;
      _dragAmount = 0;
      _dragItemId = -1;
      return;
    } else {
      cancelDrag();
      return;
    }
  } else {
    _nodes[_dragSrcIndex].content = dragCell;
  }

  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
}

void GameModel::cancelDrag() {
  if (!hasDrag())
    return;
  CellContent c;
  c.type = CellType::ITEM;
  c.data.item.id = _dragItemId;
  c.data.item.count = _dragAmount;
  _nodes[_dragSrcIndex].content = c;
  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
}

void GameModel::drainEvents(std::vector<CellEvent> &out) {
  if (_events.empty()) return;
  out.insert(out.end(), _events.begin(), _events.end());
  _events.clear();
}

void GameModel::interact(int idx) {
  if (idx <= 0 || idx >= (int)_nodes.size()) return;

  Cell &cell = _nodes[idx];

  switch (cell.content.type) {
  case CellType::EMPTY: {
    // Held item is a modifier — does NOT get consumed/dropped.
    // Spawn rule lookup uses _dragItemId as context (-1 if not holding).
    const SpawnRule *rule = _interactCfg.pickSpawn(_dragItemId);
    if (rule) {
      cell.content.type = CellType::ITEM;
      cell.content.data.item.id = rule->spawnItemId;
      cell.content.data.item.count = rule->spawnCount;
      emitEvent(CellEventType::SPAWN, idx, rule->spawnItemId);
    }
    break;
  }
  case CellType::ITEM: {
    // Check cardinal neighbors for a reaction
    int parent = cell.parentId;
    if (parent < 0) break;
    const Cell &parentCell = _nodes[parent];
    if (parentCell.content.type != CellType::GRID) break;

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
      const Cell &neighbor = _nodes[nIdx];
      if (neighbor.content.type != CellType::ITEM) continue;

      const ReactionRule *rule =
          _interactCfg.findReaction(cell.content.data.item.id, neighbor.content.data.item.id);
      if (rule) {
        // Consume both
        cell.content.type = CellType::EMPTY;
        _nodes[nIdx].content.type = CellType::EMPTY;
        // Place result in the interaction cell
        cell.content.type = CellType::ITEM;
        cell.content.data.item.id = rule->result;
        cell.content.data.item.count = rule->resultCount;
        emitEvent(CellEventType::REACTION, idx, rule->result);
        return;  // Only one reaction per interact
      }
    }
    // No reaction found — cycle item ID (1→2→3→4→5→1)
    cell.content.data.item.id = (cell.content.data.item.id % GameModel::ELEMS) + 1;
    break;
  }
  case CellType::GRID:
    // No-op for now
    break;
  case CellType::STATION: {
    // If holding an item, try to place in INPUT slot
    if (hasDrag()) {
      StationData &st = cell.content.data.station;
      int parent = cell.parentId;
      if (parent < 0) break;
      const Cell &parentCell = _nodes[parent];
      if (parentCell.content.type != CellType::GRID) break;

      int dim = parentCell.content.data.grid.gridDimension;
      int first = parentCell.content.data.grid.firstChild;
      int localIdx = idx - first;
      int startRow = localIdx / dim;
      int startCol = localIdx % dim;

      // Find INPUT cell with no item
      for (int r = 0; r < st.sizeR; r++) {
        for (int c = 0; c < st.sizeC; c++) {
          int cellIdx = first + (startRow + r) * dim + (startCol + c);
          Cell &reserved = _nodes[cellIdx];
          if (reserved.content.type == CellType::RESERVED &&
              reserved.content.data.reserved.role == ReserveRole::INPUT &&
              reserved.content.data.reserved.itemId == -1) {
            reserved.content.data.reserved.itemId = _dragItemId;
            reserved.content.data.reserved.itemCount = _dragAmount;
            _dragSrcIndex = -1;
            _dragAmount = 0;
            _dragItemId = -1;
            tryStartStation(idx);
            return;
          }
        }
      }
    }
    break;
  }
  case CellType::RESERVED: {
    ReservedData &rd = cell.content.data.reserved;
    if (rd.role == ReserveRole::OUTPUT && rd.itemId != -1) {
      // Pick up output
      _dragSrcIndex = idx;
      _dragItemId = rd.itemId;
      _dragAmount = rd.itemCount;
      rd.itemId = -1;
      rd.itemCount = 0;
    } else if (rd.role == ReserveRole::INPUT && hasDrag()) {
      // Place input
      if (rd.itemId == -1) {
        rd.itemId = _dragItemId;
        rd.itemCount = _dragAmount;
        _dragSrcIndex = -1;
        _dragAmount = 0;
        _dragItemId = -1;
        tryStartStation(rd.anchorIndex);
      }
    } else if (rd.role == ReserveRole::BUFFER) {
      // Buffer: place or pick up freely
      if (hasDrag() && rd.itemId == -1) {
        rd.itemId = _dragItemId;
        rd.itemCount = _dragAmount;
        _dragSrcIndex = -1;
        _dragAmount = 0;
        _dragItemId = -1;
      } else if (!hasDrag() && rd.itemId != -1) {
        _dragSrcIndex = idx;
        _dragItemId = rd.itemId;
        _dragAmount = rd.itemCount;
        rd.itemId = -1;
        rd.itemCount = 0;
      }
    }
    break;
  }
  }
}

int GameModel::findSpillTarget(int idx) {
  std::vector<int> candidates;

  int p = _nodes[idx].parentId;
  if (p < 0) return -1;

  int parentFirst = _nodes[p].content.data.grid.firstChild;
  int parentDim = _nodes[p].content.data.grid.gridDimension;
  int localIdx = idx - parentFirst;
  int row = localIdx / parentDim;
  int col = localIdx % parentDim;

  auto addIfEmpty = [&](int ni) {
    if (ni >= 0 && ni < (int)_nodes.size() &&
        _nodes[ni].content.type == CellType::EMPTY) {
      candidates.push_back(ni);
    }
  };

  if (row > 0) addIfEmpty(parentFirst + (row - 1) * parentDim + col);
  if (row < parentDim - 1) addIfEmpty(parentFirst + (row + 1) * parentDim + col);
  if (col > 0) addIfEmpty(parentFirst + row * parentDim + (col - 1));
  if (col < parentDim - 1) addIfEmpty(parentFirst + row * parentDim + (col + 1));

  int gp = _nodes[p].parentId;
  if (gp >= 0) {
    int gpFirst = _nodes[gp].content.data.grid.firstChild;
    int gpDim = _nodes[gp].content.data.grid.gridDimension;
    int parentLocal = p - gpFirst;
    int parentRow = parentLocal / gpDim;
    int parentCol = parentLocal % gpDim;

    auto checkGp = [&](int dr, int dc, int bRow, int bCol) {
      int gr = parentRow + dr, gc = parentCol + dc;
      if (gr < 0 || gr >= gpDim || gc < 0 || gc >= gpDim) return;
      int gi = gpFirst + gr * gpDim + gc;
      if (_nodes[gi].content.type == CellType::EMPTY) {
        candidates.push_back(gi);
      } else if (_nodes[gi].content.type == CellType::GRID) {
        int nf = _nodes[gi].content.data.grid.firstChild;
        int nd = _nodes[gi].content.data.grid.gridDimension;
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
  return candidates[(int)(prng(idx, 2) % (uint32_t)candidates.size())];
}

// Planned per-cell actions recorded in phase-1, applied serially in phase-2.
struct PlMut   { int idx; CellType type; int item; int count; CellEventType ev; };
struct PlSpill { int src, tgt, item; };

uint32_t GameModel::prng(int idx, int salt) const {
  uint32_t x = (uint32_t)idx ^ (uint32_t)salt * 0x9E3779B1u;
  x ^= _prngSeed;
  x ^= (uint32_t)_tickCounter * 0x85EBCA6Du;
  x += 0x9E3779B9u;
  x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
  x = (x ^ (x >> 13)) * 0xC2B2AE35u;
  x ^= (x >> 16);
  return x;
}

void GameModel::tick() {
  using clk = std::chrono::steady_clock;
  _tprof = TickProfile{};
  _tprof.nodes = (int)_nodes.size();
  ++_tickCounter;
  auto wall = clk::now();

  const int n = (int)_nodes.size();
  int nThreads = std::clamp((int)std::thread::hardware_concurrency(), 1, 8);
  if (n < 16384) nThreads = 1;          // small boards: thread spawn dominates
  nThreads = std::min(nThreads, std::max(1, n));

  // Phase 1 (PARALLEL, strict read-only): each thread scans a disjoint node
  // range and PLANS item mutations, spill intents, and station indices. No
  // writes at all -> no locks, no atomics, no std::rand shared state.
  struct ThreadBuf {
    std::vector<PlMut> muts;
    std::vector<PlSpill> spills;
    std::vector<int> stations;
    long items = 0;
    double spillMs = 0.0;
  };
  std::vector<ThreadBuf> bufs(nThreads);
  const int64_t bigN = n;
  auto worker = [&](int tid) {
    const int b = (int)((int64_t)tid * bigN / nThreads);
    const int e = (int)((int64_t)(tid + 1) * bigN / nThreads);
    ThreadBuf &buf = bufs[tid];
    for (int i = b; i < e; i++) {
      const Cell &c = _nodes[i];
      const CellType t = c.content.type;
      if (t == CellType::ITEM) {
        buf.items++;
        const int item = c.content.data.item.id;
        const int cnt  = c.content.data.item.count;
        if (prng(i, 0) % 100 != 0) continue;        // most cells idle
        if (prng(i, 1) & 1) {                        // shrink / destroy
          if (cnt > 1)
            buf.muts.push_back({i, CellType::ITEM, item, cnt - 1, CellEventType::SHRINK});
          else
            buf.muts.push_back({i, CellType::EMPTY, item, 0, CellEventType::DESTROY});
        } else {                                    // grow / spill
          if (cnt < 5)
            buf.muts.push_back({i, CellType::ITEM, item, cnt + 1, CellEventType::GROW});
          else {
            auto s = clk::now();
            int tgt = findSpillTarget(i);
            buf.spillMs +=
                std::chrono::duration<double, std::milli>(clk::now() - s).count();
            if (tgt != -1) buf.spills.push_back({i, tgt, item});
          }
        }
      } else if (t == CellType::STATION) {
        buf.stations.push_back(i);
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

  // Aggregate profiling counts.
  for (const auto &buf : bufs) {
    _tprof.items   += (int)buf.items;
    _tprof.spills  += (int)buf.spills.size();
    _tprof.spillMs += buf.spillMs;
  }

  // Phase 2 (SERIAL): apply mutations, then spills (first-wins per target), then
  // stations. This touches only changed cells, so it is a tiny fraction of the
  // frame. Processing buffers in thread order keeps the result deterministic.
  std::unordered_set<int> spillClaimed;
  spillClaimed.reserve((size_t)_tprof.spills);
  for (auto &buf : bufs) {
    for (const PlMut &m : buf.muts) {
      Cell &cl = _nodes[m.idx];
      if (m.ev == CellEventType::DESTROY)
        cl.content.type = CellType::EMPTY;
      else
        cl.content.data.item.count = m.count;   // GROW / SHRINK: count only
      emitEvent(m.ev, m.idx, m.item);
    }
    for (const PlSpill &sp : buf.spills) {
      if (!spillClaimed.insert(sp.tgt).second) continue;  // first-wins per target
      _nodes[sp.tgt].content.type = CellType::ITEM;
      _nodes[sp.tgt].content.data.item.id  = sp.item;
      _nodes[sp.tgt].content.data.item.count = 3;
      _nodes[sp.src].content.data.item.count = 3;
      emitEvent(CellEventType::SPILL, sp.src, sp.item);
    }
    for (int si : buf.stations) tickStation(si);
  }

  _tprof.sweepMs =
      std::chrono::duration<double, std::milli>(clk::now() - wall).count();
}

void GameModel::tickEmpty(int) {
  // No-op for now
}

void GameModel::tickGrid(int) {
  // No-op for now
}

void GameModel::tickStation(int idx) {
  Cell &cell = _nodes[idx];
  StationData &st = cell.content.data.station;

  if (st.progress <= 0) return; // idle, waiting for inputs

  st.progress--;
  if (st.progress == 0) {
    const StationRecipe *recipe = _interactCfg.findStationRecipe(st.recipeIndex);
    if (!recipe) return;

    int parent = _nodes[idx].parentId;
    if (parent < 0) return;
    const Cell &parentCell = _nodes[parent];
    if (parentCell.content.type != CellType::GRID) return;

    int dim = parentCell.content.data.grid.gridDimension;
    int first = parentCell.content.data.grid.firstChild;
    int localIdx = idx - first;
    int startRow = localIdx / dim;
    int startCol = localIdx % dim;

    // Find first OUTPUT cell and place output
    for (int r = 0; r < st.sizeR; r++) {
      for (int c = 0; c < st.sizeC; c++) {
        int cellIdx = first + (startRow + r) * dim + (startCol + c);
        Cell &reserved = _nodes[cellIdx];
        if (reserved.content.type == CellType::RESERVED &&
            reserved.content.data.reserved.role == ReserveRole::OUTPUT) {
          reserved.content.data.reserved.itemId = recipe->output;
          reserved.content.data.reserved.itemCount = recipe->outputCount;
          emitEvent(CellEventType::STATION_OUTPUT, idx, recipe->output);
          return;
        }
      }
    }
  }
}

void GameModel::tryStartStation(int anchorIdx) {
  Cell &anchor = _nodes[anchorIdx];
  if (anchor.content.type != CellType::STATION) return;
  StationData &st = anchor.content.data.station;
  if (st.progress > 0) return; // already running

  const StationRecipe *recipe = _interactCfg.findStationRecipe(st.recipeIndex);
  if (!recipe) return;

  int parent = _nodes[anchorIdx].parentId;
  if (parent < 0) return;
  const Cell &parentCell = _nodes[parent];
  if (parentCell.content.type != CellType::GRID) return;

  int dim = parentCell.content.data.grid.gridDimension;
  int first = parentCell.content.data.grid.firstChild;
  int localIdx = anchorIdx - first;
  int startRow = localIdx / dim;
  int startCol = localIdx % dim;

  // Count inputs present
  int inputsFound = 0;
  for (int r = 0; r < st.sizeR; r++) {
    for (int c = 0; c < st.sizeC; c++) {
      int cellIdx = first + (startRow + r) * dim + (startCol + c);
      const Cell &reserved = _nodes[cellIdx];
      if (reserved.content.type == CellType::RESERVED &&
          reserved.content.data.reserved.role == ReserveRole::INPUT &&
          reserved.content.data.reserved.itemId != -1) {
        inputsFound++;
      }
    }
  }

  // Start if we have enough inputs
  bool needsA = recipe->inputA >= 0;
  bool needsB = recipe->inputB >= 0;
  int required = (needsA ? 1 : 0) + (needsB ? 1 : 0);

  if (inputsFound >= required) {
    // Consume inputs
    for (int r = 0; r < st.sizeR; r++) {
      for (int c = 0; c < st.sizeC; c++) {
        int cellIdx = first + (startRow + r) * dim + (startCol + c);
        Cell &reserved = _nodes[cellIdx];
        if (reserved.content.type == CellType::RESERVED &&
            reserved.content.data.reserved.role == ReserveRole::INPUT &&
            reserved.content.data.reserved.itemId != -1) {
          reserved.content.data.reserved.itemId = -1;
          reserved.content.data.reserved.itemCount = 0;
          required--;
          if (required <= 0) break;
        }
      }
      if (required <= 0) break;
    }
    st.progress = recipe->duration;
  }
}

void GameModel::setFullState(int *inData) {
  int first = _nodes[0].content.data.grid.firstChild;
  int n = _nodes[0].content.data.grid.gridDimension * _nodes[0].content.data.grid.gridDimension;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    int id = inData[i * 2];
    int count = inData[i * 2 + 1];
    if (id == -2) {
      if (_nodes[idx].content.type != CellType::GRID) {
        _nodes[idx].content.type = CellType::GRID;
        _nodes[idx].content.data.grid.firstChild = (int)_nodes.size();
        _nodes[idx].content.data.grid.gridDimension = 3;
        _nodes[idx].content.data.grid.variation = 3;
        for (int j = 0; j < 9; j++) {
          Cell child;
          child.parentId = idx;
          child.content.type = CellType::EMPTY;
          _nodes.push_back(child);
        }
      }
    } else if (id == -1) {
      _nodes[idx].content.type = CellType::EMPTY;
    } else {
      _nodes[idx].content.type = CellType::ITEM;
      _nodes[idx].content.data.item.id = id;
      _nodes[idx].content.data.item.count = count;
    }
  }
}

void GameModel::getFullState(int *outData) const {
  int first = _nodes[0].content.data.grid.firstChild;
  int n = _nodes[0].content.data.grid.gridDimension * _nodes[0].content.data.grid.gridDimension;
  int subgridIdx = 0;
  for (int i = 0; i < n; i++) {
    int idx = first + i;
    if (_nodes[idx].content.type == CellType::ITEM) {
      outData[i * 2] = _nodes[idx].content.data.item.id;
      outData[i * 2 + 1] = _nodes[idx].content.data.item.count;
    } else if (_nodes[idx].content.type == CellType::GRID) {
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
    if (_nodes[idx].content.type == CellType::GRID) {
      if (subgridIdx == subgridSeqIndex) {
        int childFirst = _nodes[idx].content.data.grid.firstChild;
        int childSize = _nodes[idx].content.data.grid.gridDimension;
        outSize = childSize;
        int childCount = childSize * childSize;
        for (int ci = 0; ci < childCount; ci++) {
          int cidx = childFirst + ci;
          if (_nodes[cidx].content.type == CellType::ITEM) {
            outData[ci * 2] = _nodes[cidx].content.data.item.id;
            outData[ci * 2 + 1] = _nodes[cidx].content.data.item.count;
          } else if (_nodes[cidx].content.type == CellType::GRID) {
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
  if (gridIndex <= 0 || gridIndex >= (int)_nodes.size()) return false;

  const StationRecipe *recipe = _interactCfg.findStationRecipe(recipeId);
  if (!recipe) {
    fprintf(stderr, "DEBUG: recipe %d not found (have %d recipes)\n", recipeId, (int)_interactCfg.stationRecipes.size());
    return false;
  }

  Cell &anchorCell = _nodes[gridIndex];
  if (anchorCell.parentId < 0) return false;

  const Cell &parentCell = _nodes[anchorCell.parentId];
  if (parentCell.content.type != CellType::GRID) return false;

  int parentDim = parentCell.content.data.grid.gridDimension;
  int parentFirst = parentCell.content.data.grid.firstChild;
  int localIdx = gridIndex - parentFirst;
  int startRow = localIdx / parentDim;
  int startCol = localIdx % parentDim;

  // Check all cells are EMPTY and within bounds
  for (int r = 0; r < recipe->sizeR; r++) {
    for (int c = 0; c < recipe->sizeC; c++) {
      int row = startRow + r;
      int col = startCol + c;
      if (row >= parentDim || col >= parentDim) {
        fprintf(stderr, "DEBUG: out of bounds row=%d col=%d dim=%d\n", row, col, parentDim);
        return false;
      }
      int idx = parentFirst + row * parentDim + col;
      if (_nodes[idx].content.type != CellType::EMPTY) {
        fprintf(stderr, "DEBUG: cell [%d,%d] idx=%d not empty (type=%d)\n", row, col, idx, (int)_nodes[idx].content.type);
        return false;
      }
    }
  }


  // Place anchor
  anchorCell.content.type = CellType::STATION;
  anchorCell.content.byteSize = calcByteSize(CellType::STATION);
  anchorCell.content.data.station = {recipeId, 0, recipe->sizeR, recipe->sizeC};

  // Place reserved cells
  for (int r = 0; r < recipe->sizeR; r++) {
    for (int c = 0; c < recipe->sizeC; c++) {
      int row = startRow + r;
      int col = startCol + c;
      int idx = parentFirst + row * parentDim + col;
      if (idx == gridIndex) continue; // skip anchor
      int layoutIdx = r * recipe->sizeC + c;
      _nodes[idx].content.type = CellType::RESERVED;
      _nodes[idx].content.byteSize = calcByteSize(CellType::RESERVED);
      _nodes[idx].content.data.reserved = {
        gridIndex,
        recipe->layout[layoutIdx],
        -1, 0
      };
    }
  }

  return true;
}

void GameModel::removeStation(int anchorIndex) {
  if (anchorIndex <= 0 || anchorIndex >= (int)_nodes.size()) return;
  Cell &anchor = _nodes[anchorIndex];
  if (anchor.content.type != CellType::STATION) return;

  int sizeR = anchor.content.data.station.sizeR;
  int sizeC = anchor.content.data.station.sizeC;

  int parent = anchor.parentId;
  if (parent < 0) return;
  const Cell &parentCell = _nodes[parent];
  if (parentCell.content.type != CellType::GRID) return;

  int dim = parentCell.content.data.grid.gridDimension;
  int first = parentCell.content.data.grid.firstChild;
  int localIdx = anchorIndex - first;
  int startRow = localIdx / dim;
  int startCol = localIdx % dim;

  // Clear all cells
  for (int r = 0; r < sizeR; r++) {
    for (int c = 0; c < sizeC; c++) {
      int idx = first + (startRow + r) * dim + (startCol + c);
      _nodes[idx].content = CellContent{};
      _nodes[idx].content.byteSize = calcByteSize(CellType::EMPTY);
    }
  }
}
