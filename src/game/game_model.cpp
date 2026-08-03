#include "game_model.hpp"
#include <algorithm>
#include <cstdlib>
#include <ctime>

void GameModel::init(const Config &cfg) {
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

  initInteractConfig(_interactCfg, cfg.reactionsCsvPath, cfg.spawnCsvPath,
                     cfg.stationRecipesCsvPath);
}

void GameModel::initEmptyGrid(int size) {
  populateWithSubgrid(0, size);
}

void GameModel::initRandomGrid(const Config &cfg) {
  int gridChance = cfg.gridChance;

  populateWithSubgrid(0, GRID);
  int gridCount = 1;

  int itemChance = (100 - gridChance) / 2;
  int emptyChance = 100 - gridChance - itemChance;

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
    } else {
      cell.content.type = CellType::GRID;
      gridCount++;
      int subSize = std::rand() % 2 + 2;
      populateWithSubgrid(i, subSize);
    }
    i++;
  }
}

void GameModel::populateWithSubgrid(int cellIndex, int size) {
  auto &cell = _nodes[cellIndex];
  cell.content.type = CellType::GRID;
  cell.content.data.grid.gridDimension = size;
  cell.content.data.grid.firstChild = (int)_nodes.size();

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
  return candidates[std::rand() % (int)candidates.size()];
}

void GameModel::tick() {
  for (int i = 0; i < (int)_nodes.size(); i++) {
    tickCell(i);
  }
}

void GameModel::tickCell(int idx) {
  Cell &cell = _nodes[idx];
  switch (cell.content.type) {
  case CellType::EMPTY:    tickEmpty(idx); break;
  case CellType::ITEM:     tickItem(idx);  break;
  case CellType::GRID:     tickGrid(idx);  break;
  case CellType::STATION:  tickStation(idx); break;
  case CellType::RESERVED: break; // no-op
  }
}

void GameModel::tickEmpty(int) {
  // No-op for now
}

void GameModel::tickItem(int idx) {
  Cell &cell = _nodes[idx];
  if (std::rand() % 100 != 0) return;

  if (std::rand() % 2 == 0) {
    if (cell.content.data.item.count < 5) {
      cell.content.data.item.count++;
    } else {
      int target = findSpillTarget(idx);
      if (target != -1) {
        cell.content.data.item.count = 3;
        _nodes[target].content.type = CellType::ITEM;
        _nodes[target].content.data.item.id = cell.content.data.item.id;
        _nodes[target].content.data.item.count = 3;
      }
    }
  } else {
    if (cell.content.data.item.count > 1) {
      cell.content.data.item.count--;
    } else {
      cell.content.type = CellType::EMPTY;
    }
  }
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
  if (!recipe) return false;

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
      if (row >= parentDim || col >= parentDim) return false;
      int idx = parentFirst + row * parentDim + col;
      if (_nodes[idx].content.type != CellType::EMPTY) return false;
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
