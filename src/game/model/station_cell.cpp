#include "station_cell.hpp"
#include "game_model.hpp"
#include "../heat_constants.hpp"
#include <cstdio>

void StationCell::collectTickPlan(int idx, GameModel &model, std::vector<int> &stations) {
  stations.push_back(idx);
  (void)model;
}

void StationCell::tick(int idx, GameModel &model) {
  Cell &cell = model._nodes[idx];
  StationData &st = cell.content.data.station;
  if (st.progress <= 0) return;

  int heat = cell.heat;
  int steps = 1 + (heat / Heat::STATION_STEP_DIV);
  if (steps < 1) steps = 1;
  if (steps > st.progress) steps = st.progress;
  st.progress -= steps;
  model.addHeat(idx, Heat::STATION_TICK_SELF_HEAT);
  if (st.progress == 0) {
    const StationRecipe *recipe = model._interactCfg.findStationRecipe(st.recipeIndex);
    if (!recipe) return;

    int parent = model._nodes[idx].parentId;
    if (parent < 0) return;
    const Cell &parentCell = model._nodes[parent];
    if (parentCell.content.type != CellType::GRID) return;

    int dim = parentCell.content.data.grid.gridDimension;
    int first = parentCell.content.data.grid.firstChild;
    int localIdx = idx - first;
    int startRow = localIdx / dim;
    int startCol = localIdx % dim;

    for (int r = 0; r < st.sizeR; r++) {
      for (int c = 0; c < st.sizeC; c++) {
        int cellIdx = first + (startRow + r) * dim + (startCol + c);
        Cell &reserved = model._nodes[cellIdx];
        if (reserved.content.type == CellType::RESERVED &&
            reserved.content.data.reserved.role == ReserveRole::OUTPUT) {
          reserved.content.data.reserved.itemId = recipe->output;
          reserved.content.data.reserved.itemCount = recipe->outputCount;
          model.emitEvent(CellEventType::STATION_OUTPUT, idx, recipe->output);
          return;
        }
      }
    }
  }
}

void StationCell::tryStart(int anchorIdx, GameModel &model) {
  Cell &anchor = model._nodes[anchorIdx];
  if (anchor.content.type != CellType::STATION) return;
  StationData &st = anchor.content.data.station;
  if (st.progress > 0) return;

  const StationRecipe *recipe = model._interactCfg.findStationRecipe(st.recipeIndex);
  if (!recipe) return;

  int parent = model._nodes[anchorIdx].parentId;
  if (parent < 0) return;
  const Cell &parentCell = model._nodes[parent];
  if (parentCell.content.type != CellType::GRID) return;

  int dim = parentCell.content.data.grid.gridDimension;
  int first = parentCell.content.data.grid.firstChild;
  int localIdx = anchorIdx - first;
  int startRow = localIdx / dim;
  int startCol = localIdx % dim;

  int inputsFound = 0;
  for (int r = 0; r < st.sizeR; r++) {
    for (int c = 0; c < st.sizeC; c++) {
      int cellIdx = first + (startRow + r) * dim + (startCol + c);
      const Cell &reserved = model._nodes[cellIdx];
      if (reserved.content.type == CellType::RESERVED &&
          reserved.content.data.reserved.role == ReserveRole::INPUT &&
          reserved.content.data.reserved.itemId != -1) {
        inputsFound++;
      }
    }
  }

  bool needsA = recipe->inputA >= 0;
  bool needsB = recipe->inputB >= 0;
  int required = (needsA ? 1 : 0) + (needsB ? 1 : 0);

  if (inputsFound >= required) {
    for (int r = 0; r < st.sizeR; r++) {
      for (int c = 0; c < st.sizeC; c++) {
        int cellIdx = first + (startRow + r) * dim + (startCol + c);
        Cell &reserved = model._nodes[cellIdx];
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

void StationCell::interact(int idx, GameModel &model) {
  if (!model.hasDrag()) return;
  StationData &st = model._nodes[idx].content.data.station;
  int parent = model._nodes[idx].parentId;
  if (parent < 0) return;
  const Cell &parentCell = model._nodes[parent];
  if (parentCell.content.type != CellType::GRID) return;

  int dim = parentCell.content.data.grid.gridDimension;
  int first = parentCell.content.data.grid.firstChild;
  int localIdx = idx - first;
  int startRow = localIdx / dim;
  int startCol = localIdx % dim;

  for (int r = 0; r < st.sizeR; r++) {
    for (int c = 0; c < st.sizeC; c++) {
      int cellIdx = first + (startRow + r) * dim + (startCol + c);
      Cell &reserved = model._nodes[cellIdx];
      if (reserved.content.type == CellType::RESERVED &&
          reserved.content.data.reserved.role == ReserveRole::INPUT &&
          reserved.content.data.reserved.itemId == -1) {
        reserved.content.data.reserved.itemId = model._dragItemId;
        reserved.content.data.reserved.itemCount = model._dragAmount;
        model.addHeat(cellIdx, Heat::STATION_INPUT_HEAT);
        model.addHeat(idx, Heat::STATION_ANCHOR_HEAT);
        model._dragSrcIndex = -1;
        model._dragAmount = 0;
        model._dragItemId = -1;
        tryStart(idx, model);
        return;
      }
    }
  }
}

bool StationCell::place(int gridIndex, int recipeId, GameModel &model) {
  if (gridIndex <= 0 || gridIndex >= (int)model._nodes.size()) return false;

  const StationRecipe *recipe = model._interactCfg.findStationRecipe(recipeId);
  if (!recipe) {
    fprintf(stderr, "DEBUG: recipe %d not found (have %d recipes)\n", recipeId, (int)model._interactCfg.stationRecipes.size());
    return false;
  }

  Cell &anchorCell = model._nodes[gridIndex];
  if (anchorCell.parentId < 0) return false;

  const Cell &parentCell = model._nodes[anchorCell.parentId];
  if (parentCell.content.type != CellType::GRID) return false;

  int parentDim = parentCell.content.data.grid.gridDimension;
  int parentFirst = parentCell.content.data.grid.firstChild;
  int localIdx = gridIndex - parentFirst;
  int startRow = localIdx / parentDim;
  int startCol = localIdx % parentDim;

  for (int r = 0; r < recipe->sizeR; r++) {
    for (int c = 0; c < recipe->sizeC; c++) {
      int row = startRow + r;
      int col = startCol + c;
      if (row >= parentDim || col >= parentDim) {
        fprintf(stderr, "DEBUG: out of bounds row=%d col=%d dim=%d\n", row, col, parentDim);
        return false;
      }
      int idx = parentFirst + row * parentDim + col;
      if (model._nodes[idx].content.type != CellType::EMPTY) {
        fprintf(stderr, "DEBUG: cell [%d,%d] idx=%d not empty (type=%d)\n", row, col, idx, (int)model._nodes[idx].content.type);
        return false;
      }
    }
  }

  model.setType(gridIndex, CellType::STATION);
  anchorCell.content.byteSize = calcByteSize(CellType::STATION);
  anchorCell.content.data.station = {recipeId, 0, recipe->sizeR, recipe->sizeC};

  for (int r = 0; r < recipe->sizeR; r++) {
    for (int c = 0; c < recipe->sizeC; c++) {
      int row = startRow + r;
      int col = startCol + c;
      int idx = parentFirst + row * parentDim + col;
      if (idx == gridIndex) continue;
      int layoutIdx = r * recipe->sizeC + c;
      model.setType(idx, CellType::RESERVED);
      model._nodes[idx].content.byteSize = calcByteSize(CellType::RESERVED);
      model._nodes[idx].content.data.reserved = {
        gridIndex,
        recipe->layout[layoutIdx],
        -1, 0
      };
    }
  }

  return true;
}

void StationCell::remove(int anchorIndex, GameModel &model) {
  if (anchorIndex <= 0 || anchorIndex >= (int)model._nodes.size()) return;
  Cell &anchor = model._nodes[anchorIndex];
  if (anchor.content.type != CellType::STATION) return;

  int sizeR = anchor.content.data.station.sizeR;
  int sizeC = anchor.content.data.station.sizeC;

  int parent = anchor.parentId;
  if (parent < 0) return;
  const Cell &parentCell = model._nodes[parent];
  if (parentCell.content.type != CellType::GRID) return;

  int dim = parentCell.content.data.grid.gridDimension;
  int first = parentCell.content.data.grid.firstChild;
  int localIdx = anchorIndex - first;
  int startRow = localIdx / dim;
  int startCol = localIdx % dim;

  for (int r = 0; r < sizeR; r++) {
    for (int c = 0; c < sizeC; c++) {
      int idx = first + (startRow + r) * dim + (startCol + c);
      model.setContent(idx, CellContent{});
      model._nodes[idx].content.byteSize = calcByteSize(CellType::EMPTY);
    }
  }
}
