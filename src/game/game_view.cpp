#include "game_view.hpp"
#include "../shader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static constexpr int MAX_PREVIEW_DEPTH = 100;

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f}, {0.90f, 0.30f, 0.30f}, {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f}, {0.80f, 0.30f, 0.85f},
};

const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_gridBg[3] = {0.25f, 0.40f, 0.60f};

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
}

void GameView::addQuad(float cx, float cy, float w, float h,
                       const float color[3]) {
  if (cx + w <= -1.0f || cx - w >= 1.0f ||
      cy + h <= -1.0f || cy - h >= 1.0f)
    return;
  if (_v + 30 > _verts.data() + _maxVerts)
    return;
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
  float itemDotW = 0.05f * scale * _zoom;
  float itemDotH = 0.05f * scale * _aspect * _zoom;
  float spacingX = itemDotW * 2.5f;
  float spacingY = itemDotH * 2.5f;
  switch (count) {
  case 1:
    addQuad(cx, cy, itemDotW, itemDotH, color);
    break;
  case 2:
    addQuad(cx - spacingX * 0.5f, cy, itemDotW, itemDotH, color);
    addQuad(cx + spacingX * 0.5f, cy, itemDotW, itemDotH, color);
    break;
  case 3:
    addQuad(cx, cy - spacingY * 0.5f, itemDotW, itemDotH, color);
    addQuad(cx - spacingX * 0.5f, cy + spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx + spacingX * 0.5f, cy + spacingY * 0.5f, itemDotW, itemDotH,
            color);
    break;
  case 4:
    addQuad(cx - spacingX * 0.5f, cy - spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx + spacingX * 0.5f, cy - spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx - spacingX * 0.5f, cy + spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx + spacingX * 0.5f, cy + spacingY * 0.5f, itemDotW, itemDotH,
            color);
    break;
  case 5:
    addQuad(cx, cy, itemDotW, itemDotH, color);
    addQuad(cx - spacingX * 0.5f, cy - spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx + spacingX * 0.5f, cy - spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx - spacingX * 0.5f, cy + spacingY * 0.5f, itemDotW, itemDotH,
            color);
    addQuad(cx + spacingX * 0.5f, cy + spacingY * 0.5f, itemDotW, itemDotH,
            color);
    break;
  default:
    addQuad(cx, cy, itemDotW, itemDotH, color);
    break;
  }
}

void GameView::renderEmpty(float ox, float oy, float cellW, float cellH,
                           const float bgColor[3]) {
  float halfW = cellW * 0.5f;
  float halfH = cellH * 0.5f;
  addQuad(ox + halfW, oy + halfH, halfW, halfH, bgColor ? bgColor : _grey);
}

void GameView::renderItem(float ox, float oy, float cellW, float cellH,
                          int itemId, int count, float scale,
                          const float bgColor[3]) {
  float halfW = cellW * 0.5f;
  float halfH = cellH * 0.5f;
  addQuad(ox + halfW, oy + halfH, halfW, halfH, bgColor ? bgColor : _grey);
  const float *col = _elemColors[itemId];
  renderCellItems(ox + halfW, oy + halfH, count, col, scale);
}

void GameView::childCellLayout(int nodeIndex, float ox, float oy,
                               float contentW, float contentH,
                               int r, int c,
                               float& childOx, float& childOy,
                               float& childW, float& childH) const {
  const Cell& cell = _model.node(nodeIndex);
  int gridDim = cell.data.grid.gridDimension;

  childW = contentW / (gridDim + (gridDim - 1) * _gapRatio);
  childH = contentH / (gridDim + (gridDim - 1) * _gapRatio);
  float pitchX = childW * (1 + _gapRatio);
  float pitchY = childH * (1 + _gapRatio);
  float halfW = childW * 0.5f;
  float halfH = childH * 0.5f;

  float cx = ox + halfW + c * pitchX;
  float cy = oy + contentH - halfH - r * pitchY;
  childOx = cx - halfW;
  childOy = cy - halfH;
}

void GameView::renderGrid(int nodeIndex, float ox, float oy, float contentW,
                          float contentH, int depth) {
  const Cell &cell = _model.node(nodeIndex);
  int firstChild = cell.data.grid.firstChild;
  int gridDim = cell.data.grid.gridDimension;

  float g = 0.45f - std::min(depth, 3) * 0.07f;
  const float gridBg[3] = {g, g, g + 0.03f};
  float halfCW = contentW * 0.5f;
  float halfCH = contentH * 0.5f;
  addQuad(ox + halfCW, oy + halfCH, halfCW, halfCH,
           depth == 0 ? _gridBg : gridBg);

  for (int r = 0; r < gridDim; r++) {
    for (int c = 0; c < gridDim; c++) {
      int childIndex = firstChild + r * gridDim + c;
      float childOx, childOy, childW, childH;
      childCellLayout(nodeIndex, ox, oy, contentW, contentH, r, c,
                      childOx, childOy, childW, childH);
      renderCell(childIndex, childOx, childOy, childW, childH, depth + 1);
    }
  }
}

void GameView::renderCell(int nodeIndex, float ox, float oy, float cellW,
                          float cellH, int depth) {
  if (depth >= MAX_PREVIEW_DEPTH)
    return;
  if (_v + 30 > _verts.data() + _maxVerts)
    return;
  if (ox + cellW <= -1.0f || ox >= 1.0f ||
      oy + cellH <= -1.0f || oy >= 1.0f)
    return;
  constexpr float minClipSize = 0.002f;
  if (cellW < minClipSize || cellH < minClipSize)
    return;

  const Cell &cell = _model.node(nodeIndex);
  const bool isAnchor = nodeIndex == _anchorIndex;
  switch (cell.type) {
  case CellType::EMPTY:
    renderEmpty(ox, oy, cellW, cellH, isAnchor ? _gridBg : nullptr);
    break;
  case CellType::ITEM:
  { float scale = cellW / (_zoom * _cellSize);
    renderItem(ox, oy, cellW, cellH, cell.data.item.id, cell.data.item.count,
               scale, isAnchor ? _gridBg : nullptr);
    break;
  }
  case CellType::GRID:
    renderGrid(nodeIndex, ox, oy, cellW, cellH, depth);
    break;
  }
}

int GameView::resolveLeafCell(float wx, float wy) const {
  const Cell &anchor = _model.node(_anchorIndex);
  constexpr float aw = _anchorWidth;
  float ox = -aw * 0.5f, oy = -aw * 0.5f;
  if (anchor.type != CellType::GRID)
    return _anchorIndex;
  return resolveCellAt(wx, wy, _anchorIndex, _anchorSize, ox, oy, aw);
}

int GameView::resolveCellAt(float wx, float wy, int nodeIndex, int gridDim,
                            float ox, float oy, float contentW) const {
  float childCellSize = contentW / (gridDim + (gridDim - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = ox + half;
  float startY = oy + contentW - half;

  int c = (int)((wx - startX) / pitch + 0.5f);
  int r = (int)((startY - wy) / pitch + 0.5f);

  if (r < 0 || r >= gridDim || c < 0 || c >= gridDim)
    return -1;

  float cx = startX + c * pitch;
  float cy = startY - r * pitch;
  if (std::abs(wx - cx) > half || std::abs(wy - cy) > half)
    return -1;

  int childIdx = _model.node(nodeIndex).data.grid.firstChild + r * gridDim + c;
  const Cell &child = _model.node(childIdx);

  if (child.type == CellType::GRID) {
    int childDim = child.data.grid.gridDimension;
    return resolveCellAt(wx, wy, childIdx, childDim,
                         cx - half, cy - half, childCellSize);
  }

    return childIdx;
}

int GameView::resolveCenterCell(float wx, float wy) const {
  constexpr float aw = _anchorWidth;
  float ox = -aw * 0.5f, oy = -aw * 0.5f;

  if (wx >= ox && wx <= ox + aw && wy >= oy && wy <= oy + aw) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.type != CellType::GRID) return _anchorIndex;
    return resolveCellAtWithSizeCheck(wx, wy, _anchorIndex, _anchorSize, ox, oy, aw);
  }

  int parentIdx = _model.node(_anchorIndex).parent;
  if (parentIdx < 0) return _anchorIndex;

  const Cell &parent = _model.node(parentIdx);
  if (parent.type != CellType::GRID) return _anchorIndex;

  int pd = parent.data.grid.gridDimension;
  int firstChild = parent.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int anchorRow = offset / pd;
  int anchorCol = offset % pd;
  if (anchorRow < 0 || anchorRow >= pd || anchorCol < 0 || anchorCol >= pd)
    return _anchorIndex;

  float pitch = aw * (1 + _gapRatio);
  float half = aw * 0.5f;
  float cw_p = aw * (pd + (pd - 1) * _gapRatio);
  float ox_p = -half - anchorCol * pitch;
  float oy_p = -cw_p + half + anchorRow * pitch;

  if (wx < ox_p || wx > ox_p + cw_p || wy < oy_p || wy > oy_p + cw_p)
    return parentIdx;

  return resolveCellAtWithSizeCheck(wx, wy, parentIdx, pd, ox_p, oy_p, cw_p);
}

int GameView::resolveCellAtWithSizeCheck(float wx, float wy, int nodeIndex,
                                          int gridDim, float ox, float oy,
                                          float contentW) const {
  const Cell &cell = _model.node(nodeIndex);
  if (cell.type != CellType::GRID) return nodeIndex;

  float childCellSize = contentW / (gridDim + (gridDim - 1) * _gapRatio);

  constexpr float screenWidthNDC = 2.0f;
  if (childCellSize * _zoom < 0.15f * screenWidthNDC)
    return nodeIndex;

  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = ox + half;
  float startY = oy + contentW - half;

  int c = (int)((wx - startX) / pitch + 0.5f);
  int r = (int)((startY - wy) / pitch + 0.5f);

  if (r < 0 || r >= gridDim || c < 0 || c >= gridDim) return nodeIndex;

  float cx = startX + c * pitch;
  float cy = startY - r * pitch;
  if (std::abs(wx - cx) > half || std::abs(wy - cy) > half) return nodeIndex;

  int childIdx = cell.data.grid.firstChild + r * gridDim + c;
  const Cell &child = _model.node(childIdx);

  if (child.type == CellType::GRID) {
    return resolveCellAtWithSizeCheck(wx, wy, childIdx,
                                       child.data.grid.gridDimension,
                                       cx - half, cy - half, childCellSize);
  }
  return childIdx;
}

void GameView::cellWorldCenter(int targetIdx, float& wx, float& wy,
                                float& cw, float& ch) const {
  constexpr float aw = _anchorWidth;
  float ox = -aw * 0.5f;
  float oy = -aw * 0.5f;
  cw = aw, ch = aw;
  int nodeIdx = _anchorIndex;

  while (nodeIdx != targetIdx) {
    const Cell& node = _model.node(nodeIdx);
    if (node.type != CellType::GRID) break;

    int gridDim = node.data.grid.gridDimension;
    int firstChild = node.data.grid.firstChild;

    int directChild = targetIdx;
    while (_model.node(directChild).parent != nodeIdx) {
      directChild = _model.node(directChild).parent;
    }

    int offset = directChild - firstChild;
    int r = offset / gridDim;
    int c = offset % gridDim;
    if (r < 0 || r >= gridDim || c < 0 || c >= gridDim) break;

    float childOx, childOy, childW, childH;
    childCellLayout(nodeIdx, ox, oy, cw, ch, r, c,
                    childOx, childOy, childW, childH);

    if (directChild == targetIdx) {
      wx = childOx + childW * 0.5f;
      wy = childOy + childH * 0.5f;
      cw = childW;
      ch = childH;
      return;
    }

    nodeIdx = directChild;
    ox = childOx;
    oy = childOy;
    cw = childW;
    ch = childH;
  }

  wx = ox + cw * 0.5f;
  wy = oy + ch * 0.5f;
}

void GameView::focusTransform(int targetIdx) {
  float wx, wy, cw, ch;
  cellWorldCenter(targetIdx, wx, wy, cw, ch);
  (void)ch;

  const Cell &cell = _model.node(targetIdx);
  _anchorIndex = targetIdx;
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;

  _panX = wx * _zoom + _panX;
  _panY = wy * _aspect * _zoom + _panY;
  _zoom = _zoom * cw / _anchorWidth;
  if (_zoom < 0.1f) _zoom = 0.1f;
}

void GameView::resetView() {
  _anchorIndex = 0;
  const Cell &cell = _model.node(0);
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
  _zoom = 1.0f;
  _panX = 0.0f;
  _panY = 0.0f;
}

void GameView::focusCenterCell(int winW, int winH) {
  float wx, wy;
  screenToWorld(winW / 2, winH / 2, winW, winH, wx, wy);
  int idx = resolveCenterCell(wx, wy);
  if (idx < 0 || idx == _anchorIndex) return;

  if (isDescendant(_anchorIndex, idx)) {
    focusTransform(idx);
    return;
  }

  while (!isDescendant(_anchorIndex, idx)) {
    if (!unfocusOneLevel()) return;
  }
  focusTransform(idx);
}

bool GameView::unfocusOneLevel() {
  int p = _model.node(_anchorIndex).parent;
  if (p < 0) return false;

  const Cell &parent = _model.node(p);
  if (parent.type != CellType::GRID) return false;

  int gridDim = parent.data.grid.gridDimension;
  int firstChild = parent.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int r = offset / gridDim;
  int c = offset % gridDim;

  constexpr float aw = _anchorWidth;
  float childOx, childOy, childW, childH;
  childCellLayout(p, -aw * 0.5f, -aw * 0.5f, aw, aw, r, c,
                  childOx, childOy, childW, childH);

  float cx = childOx + childW * 0.5f;
  float cy = childOy + childH * 0.5f;
  float savedZoom = _zoom;

  _anchorIndex = p;
  _anchorSize = gridDim;
  _zoom = savedZoom * _anchorWidth / childW;
  if (_zoom < 0.1f) _zoom = 0.1f;
  _panX -= cx * (savedZoom * _anchorWidth / childW);
  _panY -= cy * _aspect * (savedZoom * _anchorWidth / childW);
  return true;
}

bool GameView::isDescendant(int ancestor, int node) const {
  while (node >= 0) {
    if (node == ancestor) return true;
    node = _model.node(node).parent;
  }
  return false;
}

void GameView::render(int winW, int winH) {
  _aspect = (float)winW / (float)winH;
  size_t maxFloats = (size_t)_model.totalNodes() * 200 + 1024;
  if (maxFloats > _maxVerts) maxFloats = _maxVerts;
  _verts.resize(maxFloats);
  _v = _verts.data();

  glUseProgram(_prog);

  constexpr float aw = _anchorWidth;
  float ox = -aw * 0.5f * _zoom + _panX;
  float oy = -aw * 0.5f * _aspect * _zoom + _panY;
  float cw = aw * _zoom;
  float ch = aw * _aspect * _zoom;

  const Cell &anchor = _model.node(_anchorIndex);
  if (anchor.type == CellType::GRID) {
    renderGrid(_anchorIndex, ox, oy, cw, ch, 0);
  } else {
    renderCell(_anchorIndex, ox, oy, cw, ch, 0);
  }

  if (_model.hasDrag()) {
    int dragId = _model.dragItemId();
    int dragAmount = _model.dragAmount();
    const float *col = _elemColors[dragId];

    if (!_dragWasActive) {
      _dragAnimStartTime = SDL_GetTicks();
    }

    double elapsed = (double)(SDL_GetTicks() - _dragAnimStartTime) / 1000.0;
    double t = elapsed / _dragAnimDuration;
    if (t >= 1.0) {
      t = 1.0;
    }
    float eased = (float)(t * (2.0 - t));

    float scale = (0.5f + 0.5f * eased) / _zoom;

    float sx = _dragWX * _zoom + _panX;
    float sy = _dragWY * _aspect * _zoom + _panY;
    renderCellItems(sx, sy, dragAmount, col, scale);
  }
  _dragWasActive = _model.hasDrag();

  int totalFloats = (int)(_v - _verts.data());
  if (totalFloats == 0)
    return;

  glBindBuffer(GL_ARRAY_BUFFER, _vbo);
  glBufferData(GL_ARRAY_BUFFER, totalFloats * sizeof(float), _verts.data(),
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

  float childCellSize = _anchorWidth / (size + (size - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = size * pitch;
  float halfContent = childCellSize * 0.5f;

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
  float childCellSize = _anchorWidth / (_anchorSize + (_anchorSize - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = _anchorSize * pitch;
  return -totalSize * 0.5f + pitch * 0.5f + col * pitch;
}

float GameView::gridToWorldY(int row) const {
  float childCellSize = _anchorWidth / (_anchorSize + (_anchorSize - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = _anchorSize * pitch;
  return totalSize * 0.5f - pitch * 0.5f - row * pitch;
}

void GameView::focusGrid(int nodeIndex) {
  if (nodeIndex >= 0 && _model.node(nodeIndex).type == CellType::GRID) {
    focusTransform(nodeIndex);
  }
}

void GameView::unfocusGrid() {
  unfocusOneLevel();
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
  float dx = 2.0f * (px - _lastPanX) / (float)winW;
  float dy = -2.0f * (py - _lastPanY) / (float)winH;
  _panX += dx;
  _panY += dy;
  _lastPanX = px;
  _lastPanY = py;
}

void GameView::endPan() { _isPanning = false; }
