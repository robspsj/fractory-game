#include "game_view.hpp"
#include "../shader.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

static constexpr int MAX_PREVIEW_DEPTH = 3;

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f}, {0.90f, 0.30f, 0.30f}, {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f}, {0.80f, 0.30f, 0.85f},
};

const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_hoverBg[3] = {0.7f, 0.7f, 0.7f};
const float GameView::_gridBg[3] = {0.8f, 0.2f, 0.2f};

GameView::GameView(GameModel &model) : _model(model) {
  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.size : GameModel::GRID;
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

void GameView::renderEmpty(float ox, float oy, float cellSize,
                           const float bg[3]) {
  float half = cellSize * 0.5f;
  addQuad(ox + half, oy + half, half, half, bg);
}

void GameView::renderItem(float ox, float oy, float cellSize, int itemId,
                          int count, const float bg[3], float scale) {
  float half = cellSize * 0.5f;
  addQuad(ox + half, oy + half, half, half, bg);
  const float *col = _elemColors[itemId];
  renderCellItems(ox + half, oy + half, count, col, scale);
}

void GameView::renderGrid(int nodeIndex, float ox, float oy, float width,
                          float pitch, float cellSize, int depth) {
  const Cell &cell = _model.node(nodeIndex);

  if (!(pitch > _gap * 3.0f && depth < MAX_PREVIEW_DEPTH)) {
    if (cellSize > 0) {
      float cx = ox + cellSize * 0.5f;
      float cy = oy + cellSize * 0.5f;
      addQuad(cx, cy, cellSize * 0.25f, cellSize * 0.25f, _gridBg);
    }
    return;
  }

  int firstChild = cell.data.grid.firstChild;
  int size = cell.data.grid.size;

  float cs = pitch - _gap;
  float half = cs * 0.5f;
  if (half < 0.001f)
    half = pitch * 0.45f;
  cs = half * 2.0f;

  float startX = ox + half;
  float startY = oy + width - half;

  int hoverIdx = (_hoverRow >= 0) ? _model.rootChild(_hoverRow, _hoverCol) : -1;

  for (int r = 0; r < size; r++) {
    for (int c = 0; c < size; c++) {
      float childCx = startX + c * pitch;
      float childCy = startY - r * pitch;
      int childIndex = firstChild + r * size + c;
      const float *bg = (childIndex == hoverIdx) ? _hoverBg : _grey;

      if (depth + 1 < MAX_PREVIEW_DEPTH && pitch > _gap * 3.0f) {
        const Cell &childNode = _model.node(childIndex);
        if (childNode.type == CellType::GRID) {
          int childSize = childNode.data.grid.size;
          float childPitch = pitch / childSize;
          float childWidth = childSize * childPitch - _gap;
          renderCell(childIndex, childCx - childWidth * 0.5f,
                     childCy - childWidth * 0.5f, childWidth, childPitch, 0,
                     nullptr, depth + 1);
          continue;
        }
      }
      renderCell(childIndex, childCx - half, childCy - half, 0, 0, cs, bg,
                 depth + 1);
    }
  }
}

void GameView::renderCell(int nodeIndex, float ox, float oy, float width,
                          float pitch, float cellSize, const float bg[3],
                          int depth) {
  const Cell &cell = _model.node(nodeIndex);
  switch (cell.type) {
  case CellType::EMPTY:
    renderEmpty(ox, oy, cellSize, bg);
    break;
  case CellType::ITEM:
    renderItem(ox, oy, cellSize, cell.data.item.id, cell.data.item.count, bg,
               pitch > 0 ? pitch / _cellSize : 1.0f);
    break;
  case CellType::GRID:
    renderGrid(nodeIndex, ox, oy, width, pitch, cellSize, depth);
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

  float pitch = _cellSize;
  float width = _anchorSize * pitch - _gap;
  float ox = -width * 0.5f, oy = -width * 0.5f;
  renderCell(_anchorIndex, ox, oy, width, pitch, 0, nullptr, 0);

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
  float halfContent = (pitch - _gap) * 0.5f;
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
        (cell.type == CellType::GRID) ? cell.data.grid.size : GameModel::GRID;
  }
}

void GameView::unfocusGrid() {
  if (!_focusStack.empty()) {
    _focusStack.pop();
    _anchorIndex = _focusStack.empty() ? 0 : _focusStack.top();
    const Cell &cell = _model.node(_anchorIndex);
    _anchorSize =
        (cell.type == CellType::GRID) ? cell.data.grid.size : GameModel::GRID;
  }
}

void GameView::focusOffset(int delta) {
  int total = _model.totalNodes();
  _anchorIndex = std::clamp(_anchorIndex + delta, 0, total - 1);
  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.size : GameModel::GRID;
}

void GameView::zoom(float factor) {
  _zoom *= factor;
  if (_zoom < 0.1f)
    _zoom = 0.1f;
  if (_zoom > 10.0f)
    _zoom = 10.0f;
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
