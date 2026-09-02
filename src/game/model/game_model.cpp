#include "game_model.hpp"
#include "empty_cell.hpp"
#include "grid_cell.hpp"
#include "item_cell.hpp"
#include "reserved_cell.hpp"
#include "station_cell.hpp"
#include "tick_plan.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <unordered_set>

void GameModel::init(const Config &cfg) {
  _prngSeed = (cfg.seed != 0) ? (unsigned)cfg.seed : 0x9E3779B9u;
  _tickCounter = 0;
  _numItems = (cfg.numItems > 0) ? cfg.numItems : GameModel::ELEMS;
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
      cell.content.data.item.id = std::rand() % _numItems;
      cell.content.data.item.count = std::rand() % 4 + 1;
    } else if (randVal < emptyChance + itemChance + stationChance) {
      if (tryPlaceRandomStation(i)) {
        const StationData &st = _nodes[i].content.data.station;
        i += st.sizeR * st.sizeC - 1;
      } else {
        cell.content.type = CellType::EMPTY;
      }
    } else {
      cell.content.type = CellType::GRID;
      gridCount++;
      int subSize = std::rand() % 6 + 2;
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

  int recipeIdx = std::rand() % (int)_interactCfg.stationRecipes.size();
  const StationRecipe &recipe = _interactCfg.stationRecipes[recipeIdx];

  if (startRow + recipe.sizeR > parentDim) return false;
  if (startCol + recipe.sizeC > parentDim) return false;

  for (int r = 0; r < recipe.sizeR; r++) {
    for (int c = 0; c < recipe.sizeC; c++) {
      int idx = parentFirst + (startRow + r) * parentDim + (startCol + c);
      if (_nodes[idx].content.type != CellType::EMPTY) return false;
    }
  }

  cell.content.type = CellType::STATION;
  cell.content.byteSize = calcByteSize(CellType::STATION);
  cell.content.data.station = {recipe.id, 0, recipe.sizeR, recipe.sizeC};

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
  GridCell::populateWithSubgrid(cellIndex, size, _nodes);
}

void GameModel::pickUp(int nodeIndex, int amount) {
  if (nodeIndex <= 0 || nodeIndex >= (int)_nodes.size())
    return;
  if (_nodes[nodeIndex].content.type != CellType::ITEM)
    return;

  _dragSrcIndex = nodeIndex;
  _dragItemId = _nodes[nodeIndex].content.data.item.id;
  _dragAmount = std::min(amount, _nodes[nodeIndex].content.data.item.count);
  _dragHeat = _nodes[nodeIndex].heat;
  _nodes[nodeIndex].content.data.item.count -= _dragAmount;
  if (_nodes[nodeIndex].content.data.item.count <= 0) {
    _nodes[nodeIndex].content.type = CellType::EMPTY;
    _nodes[nodeIndex].content.byteSize = calcByteSize(CellType::EMPTY);
  }
}

void GameModel::drop(int nodeIndex) {
  if (!hasDrag())
    return;

  CellContent dragCell;
  dragCell.type = CellType::ITEM;
  dragCell.data.item.id = _dragItemId;
  dragCell.data.item.count = _dragAmount;
  dragCell.byteSize = calcByteSize(CellType::ITEM);

  if (nodeIndex > 0 && nodeIndex < (int)_nodes.size()) {
    if (_nodes[nodeIndex].content.type == CellType::ITEM &&
        _nodes[nodeIndex].content.data.item.id == _dragItemId) {
      _nodes[nodeIndex].content.data.item.count += _dragAmount;
      addHeat(nodeIndex, (int)_dragHeat / 4);
    } else if (_nodes[nodeIndex].content.type == CellType::EMPTY) {
      _nodes[nodeIndex].content = dragCell;
      // transfer heat with the item
      _nodes[nodeIndex].heat = _dragHeat;
    } else if (_nodes[nodeIndex].content.type == CellType::ITEM) {
      CellContent temp = _nodes[nodeIndex].content;
      uint8_t targetHeat = _nodes[nodeIndex].heat;
      _nodes[nodeIndex].content = dragCell;
      _nodes[nodeIndex].heat = _dragHeat;
      _nodes[_dragSrcIndex].content = temp;
      _nodes[_dragSrcIndex].heat = targetHeat;
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
    _nodes[_dragSrcIndex].content = dragCell;
    _nodes[_dragSrcIndex].content.byteSize = calcByteSize(CellType::ITEM);
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
  c.type = CellType::ITEM;
  c.data.item.id = _dragItemId;
  c.data.item.count = _dragAmount;
  c.byteSize = calcByteSize(CellType::ITEM);
  _nodes[_dragSrcIndex].content = c;
  // heat stays with cell, dragged heat is just visual; no need to restore
  _dragSrcIndex = -1;
  _dragAmount = 0;
  _dragItemId = -1;
  _dragHeat = 0;
}

void GameModel::drainEvents(std::vector<CellEvent> &out) {
  if (_events.empty()) return;
  out.insert(out.end(), _events.begin(), _events.end());
  _events.clear();
}

void GameModel::interact(int idx) {
  if (idx <= 0 || idx >= (int)_nodes.size()) return;

  CellType t = _nodes[idx].content.type;
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
        ItemCell::collectTickPlan(i, c, *this, buf.muts, buf.spills, buf.spillMs, buf.items);
      } else if (t == CellType::STATION) {
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

  for (const auto &buf : bufs) {
    _tprof.items   += (int)buf.items;
    _tprof.spills  += (int)buf.spills.size();
    _tprof.spillMs += buf.spillMs;
  }

  std::unordered_set<int> spillClaimed;
  spillClaimed.reserve((size_t)_tprof.spills);
  for (auto &buf : bufs) {
    for (const PlMut &m : buf.muts) {
      Cell &cl = _nodes[m.idx];
      if (m.ev == CellEventType::DESTROY)
        cl.content.type = CellType::EMPTY;
      else
        cl.content.data.item.count = m.count;
      emitEvent(m.ev, m.idx, m.item);
      // heat feedback from item activity
      if (cl.content.type != CellType::GRID) {
        int dh = 2 + (cl.heat >> 6); // a bit more when already hot
        int nh = (int)cl.heat + dh;
        if (nh > 255) nh = 255;
        cl.heat = (uint8_t)nh;
      }
    }
    for (const PlSpill &sp : buf.spills) {
      if (!spillClaimed.insert(sp.tgt).second) continue;
      _nodes[sp.tgt].content.type = CellType::ITEM;
      _nodes[sp.tgt].content.data.item.id  = sp.item;
      _nodes[sp.tgt].content.data.item.count = 3;
      _nodes[sp.src].content.data.item.count = 3;
      emitEvent(CellEventType::SPILL, sp.src, sp.item);
      // spill heats both cells a little
      addHeat(sp.src, 6);
      addHeat(sp.tgt, 6);
    }
    for (int si : buf.stations) tickStation(si);
  }

  // Heat decay & slight diffusion — keep cheap, run sequentially.
  // Decay: ~8% chance per tick per hot non-GRID cell to cool by 1.
  // Diffusion: ~3% chance to bleed 1 heat to a random sibling/neighbor empty/item/reserved cell.
  for (int i = 1; i < n; i++) {
    Cell &c = _nodes[i];
    if (c.content.type == CellType::GRID) continue;
    if (c.heat == 0) continue;
    uint32_t r = prng(i, 0x9e37 + _tickCounter);
    if ((r % 100) < 8) {
      if (c.heat > 0) c.heat--;
    }
    if ((r % 100) < 3 && c.heat > 1) {
      int tgt = findSpillTarget(i);
      // fallback: random sibling if spill target not found
      if (tgt < 0) {
        int p = c.parentId;
        if (p >= 0 && _nodes[p].content.type == CellType::GRID) {
          int dim = _nodes[p].content.data.grid.gridDimension;
          int first = _nodes[p].content.data.grid.firstChild;
          int cnt = dim * dim;
          int tries = 0;
          while (tries < 4) {
            int cand = first + (int)(prng(i + tries, 0x1234) % (uint32_t)cnt);
            if (cand != i && _nodes[cand].content.type != CellType::GRID) { tgt = cand; break; }
            tries++;
          }
        }
      }
      if (tgt >= 0 && tgt < n && _nodes[tgt].content.type != CellType::GRID) {
        c.heat--;
        uint8_t &th = _nodes[tgt].heat;
        if (th < 255) th++;
      }
    }
  }

  _tprof.sweepMs =
      std::chrono::duration<double, std::milli>(clk::now() - wall).count();
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
  return StationCell::place(gridIndex, recipeId, *this);
}

void GameModel::removeStation(int anchorIndex) {
  StationCell::remove(anchorIndex, *this);
}

uint8_t GameModel::heatAt(int idx) const {
  if (idx < 0 || idx >= (int)_nodes.size()) return 0;
  if (_nodes[idx].content.type == CellType::GRID) return 0;
  return _nodes[idx].heat;
}
void GameModel::setHeat(int idx, uint8_t h) {
  if (idx < 0 || idx >= (int)_nodes.size()) return;
  if (_nodes[idx].content.type == CellType::GRID) return;
  _nodes[idx].heat = h;
}
void GameModel::addHeat(int idx, int delta) {
  if (idx < 0 || idx >= (int)_nodes.size()) return;
  Cell &c = _nodes[idx];
  if (c.content.type == CellType::GRID) return;
  int nh = (int)c.heat + delta;
  if (nh < 0) nh = 0;
  if (nh > 255) nh = 255;
  c.heat = (uint8_t)nh;
}
void GameModel::setHeatBulk(const std::vector<std::pair<int,uint8_t>> &pairs) {
  for (auto &p : pairs) setHeat(p.first, p.second);
}
