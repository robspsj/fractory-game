#include "game_view.hpp"
#include "../shader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static constexpr int MAX_PREVIEW_DEPTH = 10;

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f}, {0.90f, 0.30f, 0.30f}, {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f}, {0.80f, 0.30f, 0.85f},
};

const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_gridBg[3] = {0.8f, 0.2f, 0.2f};

GameView::GameView(GameModel &model) : _model(model) {
  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
}

GameView::~GameView() {
  if (_prog)
    glDeleteProgram(_prog);
  if (_vbo)
    glDeleteBuffers(1, &_vbo);
}

void GameView::initGL() {
  _prog = glCreateProgram();
  glAttachShader(_prog, compile(GL_VERTEX_SHADER, vertSrc));
  glAttachShader(_prog, compile(GL_FRAGMENT_SHADER, fragSrc));
  glLinkProgram(_prog);
  {
    GLint linked;
    glGetProgramiv(_prog, GL_LINK_STATUS, &linked);
    if (!linked) {
      char log[512];
      glGetProgramInfoLog(_prog, sizeof(log), 0, log);
      fprintf(stderr, "game program link error: %s\n", log);
    }
  }
  glUseProgram(_prog);

  glGenBuffers(1, &_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, _vbo);

  _aPosLoc = glGetAttribLocation(_prog, "aPos");
  _aColorLoc = glGetAttribLocation(_prog, "aColor");
  _uPosLoc = glGetUniformLocation(_prog, "uPos");
  _uZoomLoc = glGetUniformLocation(_prog, "uZoom");
}

void GameView::addQuad(float cx, float cy, float w, float h,
                       const float color[3]) {
  for (int i = 0; i < 6; i++) {
    float dx = (i == 1 || i == 3 || i == 4) ? w : -w;
    float dy = (i == 0 || i == 1 || i == 4) ? -h : h;
    _v[0] = cx + dx;
    _v[1] = cy + dy;
    _v[2] = color[0];
    _v[3] = color[1];
    _v[4] = color[2];
    _v += 5;
  }
}

void GameView::renderCellItems(float cx, float cy, int count,
                               const float color[3], float scale) {
  if (count <= 0)
    return;
  float itemDotSize = 0.05f * scale;
  float spacing = itemDotSize * 2.5f;
  switch (count) {
  case 1:
    addQuad(cx, cy, itemDotSize, itemDotSize, color);
    break;
  case 2:
    addQuad(cx - spacing * 0.5f, cy, itemDotSize, itemDotSize, color);
    addQuad(cx + spacing * 0.5f, cy, itemDotSize, itemDotSize, color);
    break;
  case 3:
    addQuad(cx, cy - spacing * 0.5f, itemDotSize, itemDotSize, color);
    addQuad(cx - spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx + spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    break;
  case 4:
    addQuad(cx - spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx + spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx - spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx + spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    break;
  case 5:
    addQuad(cx, cy, itemDotSize, itemDotSize, color);
    addQuad(cx - spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx + spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx - spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    addQuad(cx + spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize,
            color);
    break;
  default:
    addQuad(cx, cy, itemDotSize, itemDotSize, color);
    break;
  }
}

void GameView::renderEmpty(float ox, float oy, float cellSize) {
  float half = cellSize * 0.5f;
  addQuad(ox + half, oy + half, half, half, _grey);
}

void GameView::renderItem(float ox, float oy, float cellSize, int itemId,
                          int count, float scale) {
  float half = cellSize * 0.5f;
  addQuad(ox + half, oy + half, half, half, _grey);
  const float *col = _elemColors[itemId];
  renderCellItems(ox + half, oy + half, count, col, scale);
}

void GameView::renderGrid(int nodeIndex, float ox, float oy, float contentWidth,
                          int depth) {
  const Cell &cell = _model.node(nodeIndex);
  int firstChild = cell.data.grid.firstChild;
  int gridDim = cell.data.grid.gridDimension;

  float childCellSize =
      contentWidth / (gridDim + (gridDim - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;

  float g = 0.45f - std::min(depth, 3) * 0.07f;
  const float gridBg[3] = {g, g, g + 0.03f};
  float halfWidth = contentWidth * 0.5f;
  addQuad(ox + halfWidth, oy + halfWidth, halfWidth, halfWidth, gridBg);

  float startX = ox + half;
  float startY = oy + contentWidth - half;

  for (int r = 0; r < gridDim; r++) {
    for (int c = 0; c < gridDim; c++) {
      float childCx = startX + c * pitch;
      float childCy = startY - r * pitch;
      int childIndex = firstChild + r * gridDim + c;

      renderCell(childIndex, childCx - half, childCy - half, childCellSize,
                 depth + 1);
    }
  }
}

void GameView::renderCell(int nodeIndex, float ox, float oy, float cellSize,
                          int depth) {
  const Cell &cell = _model.node(nodeIndex);
  switch (cell.type) {
  case CellType::EMPTY:
    renderEmpty(ox, oy, cellSize);
    break;
  case CellType::ITEM:
    renderItem(ox, oy, cellSize, cell.data.item.id, cell.data.item.count,
               cellSize / _cellSize);
    break;
  case CellType::GRID:
    renderGrid(nodeIndex, ox, oy, cellSize, depth);
    break;
  }
}

void GameView::render(int winW, int winH) {
  float aspect = (float)winW / (float)winH;
  static float verts[1024 * 1024];
  _v = verts;

  glUseProgram(_prog);
  glUniform2f(_uPosLoc, _panX, _panY);
  glUniform1f(_uZoomLoc, _zoom);
  glUniform1f(glGetUniformLocation(_prog, "uAspect"), aspect);

  float cellWidth = _cellSize / (1 + _gapRatio);
  float gridWidth =
      _anchorSize * cellWidth + (_anchorSize - 1) * cellWidth * _gapRatio;
  float ox = -gridWidth * 0.5f, oy = -gridWidth * 0.5f;

  const Cell &anchor = _model.node(_anchorIndex);
  if (anchor.type == CellType::GRID) {
    renderGrid(_anchorIndex, ox, oy, gridWidth, 0);
  } else {
    renderCell(_anchorIndex, ox, oy, cellWidth, 0);
  }

  if (_model.hasDrag()) {
    int dragId = _model.dragItemId();
    int dragAmount = _model.dragAmount();
    const float *col = _elemColors[dragId];
    renderCellItems(_dragWX, _dragWY, dragAmount, col);
  }

  int totalFloats = (int)(_v - verts);
  if (totalFloats == 0)
    return;

  glBindBuffer(GL_ARRAY_BUFFER, _vbo);
  glBufferData(GL_ARRAY_BUFFER, totalFloats * sizeof(float), verts,
               GL_STREAM_DRAW);

  glVertexAttribPointer(_aPosLoc, 2, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float),
                        0);
  glEnableVertexAttribArray(_aPosLoc);
  glVertexAttribPointer(_aColorLoc, 3, GL_FLOAT, GL_FALSE,
                        5 * (int)sizeof(float), (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(_aColorLoc);

  glDrawArrays(GL_TRIANGLES, 0, totalFloats / 5);

  glDisableVertexAttribArray(_aColorLoc);
  glDisableVertexAttribArray(_aPosLoc);
}

bool GameView::screenToGrid(int px, int py, int winW, int winH, int &row,
                            int &col) const {
  float wx, wy;
  screenToWorld(px, py, winW, winH, wx, wy);

  int size = _anchorSize;

  float pitch = _cellSize;
  float totalSize = size * pitch;
  float halfContent = pitch / (1 + _gapRatio) * 0.5f;
  if (halfContent < 0.001f)
    halfContent = pitch * 0.45f;

  float startX = -totalSize * 0.5f + pitch * 0.5f;
  float startY = totalSize * 0.5f - pitch * 0.5f;

  col = (int)((wx - startX) / pitch + 0.5f);
  row = (int)((startY - wy) / pitch + 0.5f);

  if (row < 0 || row >= size || col < 0 || col >= size)
    return false;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(wx - centerX) > halfContent ||
      std::abs(wy - centerY) > halfContent)
    return false;

  return true;
}

void GameView::screenToWorld(int px, int py, int winW, int winH, float &wx,
                             float &wy) const {
  float aspect = (float)winW / (float)winH;
  wx = ((2.0f * px / (float)winW) - 1.0f - _panX) / _zoom;
  wy = ((1.0f - (2.0f * py / (float)winH)) - _panY) / aspect / _zoom;
}

float GameView::gridToWorldX(int col) const {
  float pitch = _cellSize;
  float totalSize = _anchorSize * pitch;
  return -totalSize * 0.5f + pitch * 0.5f + col * pitch;
}

float GameView::gridToWorldY(int row) const {
  float pitch = _cellSize;
  float totalSize = _anchorSize * pitch;
  return totalSize * 0.5f - pitch * 0.5f - row * pitch;
}

void GameView::focusGrid(int nodeIndex) {
  if (nodeIndex >= 0 && _model.node(nodeIndex).type == CellType::GRID) {
    _focusStack.push(nodeIndex);
    _anchorIndex = nodeIndex;
    const Cell &cell = _model.node(_anchorIndex);
    _anchorSize =
        (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
  }
}

void GameView::unfocusGrid() {
  if (!_focusStack.empty()) {
    _focusStack.pop();
    _anchorIndex = _focusStack.empty() ? 0 : _focusStack.top();
    const Cell &cell = _model.node(_anchorIndex);
    _anchorSize =
        (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
  }
}

void GameView::focusOffset(int delta) {
  int total = _model.totalNodes();
  _anchorIndex = std::clamp(_anchorIndex + delta, 0, total - 1);
  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
}

void GameView::zoom(float factor, float mouseNX, float mouseNY) {
  float oldZoom = _zoom;
  _zoom *= factor;
  if (_zoom < 0.1f)
    _zoom = 0.1f;
  float actualFactor = _zoom / oldZoom;
  _panX = mouseNX * (1.0f - actualFactor) + _panX * actualFactor;
  _panY = mouseNY * (1.0f - actualFactor) + _panY * actualFactor;
}

void GameView::startPan(int px, int py) {
  _isPanning = true;
  _lastPanX = px;
  _lastPanY = py;
}

void GameView::continuePan(int px, int py, int winW, int winH) {
  float dx = (2.0f * (px - _lastPanX) / (float)winW) / _zoom;
  float dy = (-2.0f * (py - _lastPanY) / (float)winH) / _zoom;
  _panX += dx;
  _panY += dy;
  _lastPanX = px;
  _lastPanY = py;
}

void GameView::endPan() { _isPanning = false; }
