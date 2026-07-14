#include "game_view.hpp"
#include "config.hpp"
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
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parent >= 0; i = _model.node(i).parent)
    _anchorDepth++;
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

void GameView::addQuad(const Rect& r, const float color[3]) {
  if (r.ox + r.w <= -1.0f || r.ox >= 1.0f ||
      r.oy + r.h <= -1.0f || r.oy >= 1.0f)
    return;
  if (_v + 30 > _verts.data() + _maxVerts)
    return;
  float cx = r.cx(), cy = r.cy();
  float hw = r.halfW(), hh = r.halfH();
  for (int i = 0; i < 6; i++) {
    float deltaX = (i == 1 || i == 3 || i == 4) ? hw : -hw;
    float deltaY = (i == 0 || i == 1 || i == 4) ? -hh : hh;
    _v[0] = cx + deltaX;
    _v[1] = cy + deltaY;
    _v[2] = color[0];
    _v[3] = color[1];
    _v[4] = color[2];
    _v += 5;
  }
}

void GameView::renderCellItems(float centerX, float centerY, int count,
                               const float color[3], float scale) {
  if (count <= 0)
    return;
  float itemDotW = 0.05f * scale * _zoom;
  float itemDotH = 0.05f * scale * _aspect * _zoom;
  float spacingX = itemDotW * 2.5f;
  float spacingY = itemDotH * 2.5f;
  auto dot = [&](float cx, float cy) {
    addQuad({cx - itemDotW, cy - itemDotH, itemDotW * 2, itemDotH * 2}, color);
  };
  switch (count) {
  case 1:
    dot(centerX, centerY);
    break;
  case 2:
    dot(centerX - spacingX * 0.5f, centerY);
    dot(centerX + spacingX * 0.5f, centerY);
    break;
  case 3:
    dot(centerX, centerY - spacingY * 0.5f);
    dot(centerX - spacingX * 0.5f, centerY + spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY + spacingY * 0.5f);
    break;
  case 4:
    dot(centerX - spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX - spacingX * 0.5f, centerY + spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY + spacingY * 0.5f);
    break;
  case 5:
    dot(centerX, centerY);
    dot(centerX - spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY - spacingY * 0.5f);
    dot(centerX - spacingX * 0.5f, centerY + spacingY * 0.5f);
    dot(centerX + spacingX * 0.5f, centerY + spacingY * 0.5f);
    break;
  default:
    dot(centerX, centerY);
    break;
  }
}

void GameView::renderEmpty(const Rect& r, const float bgColor[3]) {
  addQuad(r, bgColor ? bgColor : _grey);
}

void GameView::renderItem(const Rect& r, int itemId, int count, float scale,
                          const float bgColor[3]) {
  addQuad(r, bgColor ? bgColor : _grey);
  const float *col = _elemColors[itemId];
  renderCellItems(r.cx(), r.cy(), count, col, scale);
}

Rect GameView::childCellLayout(int nodeIndex, const Rect& r, int row, int col) const {
  const Cell& cell = _model.node(nodeIndex);
  int gridDim = cell.data.grid.gridDimension;

  float childW = r.w / (gridDim + (gridDim - 1) * _gapRatio);
  float childH = r.h / (gridDim + (gridDim - 1) * _gapRatio);
  float pitchX = childW * (1 + _gapRatio);
  float pitchY = childH * (1 + _gapRatio);
  float halfW = childW * 0.5f;
  float halfH = childH * 0.5f;

  float centerX = r.ox + halfW + col * pitchX;
  float centerY = r.oy + r.h - halfH - row * pitchY;
  return {centerX - halfW, centerY - halfH, childW, childH};
}

void GameView::renderGrid(int nodeIndex, const Rect& r, int depth, int excludeChild) {
  const Cell &cell = _model.node(nodeIndex);
  int firstChild = cell.data.grid.firstChild;
  int gridDim = cell.data.grid.gridDimension;

  float grayValue = 0.45f - std::min(depth, 3) * 0.07f;
  const float gridBg[3] = {grayValue, grayValue, grayValue + 0.03f};
  addQuad(r, depth == 0 ? _gridBg : gridBg);

  for (int row = 0; row < gridDim; row++) {
    for (int col = 0; col < gridDim; col++) {
      int childIndex = firstChild + row * gridDim + col;
      if (childIndex == excludeChild)
        continue;
      Rect child = childCellLayout(nodeIndex, r, row, col);
      renderCell(childIndex, child, depth + 1);
    }
  }
}

void GameView::renderCell(int nodeIndex, const Rect& r, int depth) {
  if (depth >= MAX_PREVIEW_DEPTH)
    return;
  if (_v + 30 > _verts.data() + _maxVerts)
    return;
  if (r.outsideClip())
    return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize)
    return;

  const Cell &cell = _model.node(nodeIndex);
  const bool isAnchor = nodeIndex == _anchorIndex;
  switch (cell.type) {
  case CellType::EMPTY:
    renderEmpty(r, isAnchor ? _gridBg : nullptr);
    break;
  case CellType::ITEM:
  { float scale = r.w / (_zoom * _cellSize);
    renderItem(r, cell.data.item.id, cell.data.item.count,
               scale, isAnchor ? _gridBg : nullptr);
    break;
  }
  case CellType::GRID:
    renderGrid(nodeIndex, r, depth);
    break;
  }
}

int GameView::resolveLeafCell(float worldX, float worldY) const {
  constexpr float anchorW = _anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  if (r.contains(worldX, worldY)) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.type != CellType::GRID) return _anchorIndex;
    return resolveCellAt(worldX, worldY, _anchorIndex, _anchorSize, r);
  }

  int currIdx = _anchorIndex;
  Rect currR = r;

  while (true) {
    int parentIdx = _model.node(currIdx).parent;
    if (parentIdx < 0) break;

    const Cell &parent = _model.node(parentIdx);
    if (parent.type != CellType::GRID) break;

    int parentDim = parent.data.grid.gridDimension;
    int firstChild = parent.data.grid.firstChild;
    int offset = currIdx - firstChild;
    int row = offset / parentDim;
    int col = offset % parentDim;
    if (row < 0 || row >= parentDim || col < 0 || col >= parentDim) break;

    float k = (float)(parentDim + (parentDim - 1) * _gapRatio);
    float pitchX = currR.w * (1 + _gapRatio);
    float pitchY = currR.h * (1 + _gapRatio);
    Rect parentR = {
      currR.ox - col * pitchX,
      currR.oy + currR.h - currR.h * k + row * pitchY,
      currR.w * k,
      currR.h * k
    };

    if (parentR.contains(worldX, worldY))
      return resolveCellAt(worldX, worldY, parentIdx, parentDim, parentR);

    currIdx = parentIdx;
    currR = parentR;
  }

  return -1;
}

int GameView::resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim,
                            const Rect& r) const {
  float childCellSize = r.w / (gridDim + (gridDim - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = r.ox + half;
  float startY = r.oy + r.w - half;

  int col = (int)((worldX - startX) / pitch + 0.5f);
  int row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= gridDim || col < 0 || col >= gridDim)
    return -1;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > half || std::abs(worldY - centerY) > half)
    return -1;

  int childIdx = _model.node(nodeIndex).data.grid.firstChild + row * gridDim + col;
  const Cell &child = _model.node(childIdx);

  if (child.type == CellType::GRID) {
    int childDim = child.data.grid.gridDimension;
    return resolveCellAt(worldX, worldY, childIdx, childDim,
                         {centerX - half, centerY - half, childCellSize, childCellSize});
  }

    return childIdx;
}

int GameView::resolveCenterCell(float worldX, float worldY) const {
  constexpr float anchorW = _anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  if (anchorR.contains(worldX, worldY)) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.type != CellType::GRID) return _anchorIndex;
    return resolveCellAtWithSizeCheck(worldX, worldY, _anchorIndex, _anchorSize, anchorR);
  }

  int parentIdx = _model.node(_anchorIndex).parent;
  if (parentIdx < 0) return _anchorIndex;

  const Cell &parent = _model.node(parentIdx);
  if (parent.type != CellType::GRID) return _anchorIndex;

  int parentDim = parent.data.grid.gridDimension;
  int firstChild = parent.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int anchorRow = offset / parentDim;
  int anchorCol = offset % parentDim;
  if (anchorRow < 0 || anchorRow >= parentDim || anchorCol < 0 || anchorCol >= parentDim)
    return _anchorIndex;

  float parentContentW = anchorW * (parentDim + (parentDim - 1) * _gapRatio);
  Rect child = childCellLayout(parentIdx, {0, 0, parentContentW, parentContentW}, anchorRow, anchorCol);
  Rect parentR = {-anchorW * 0.5f - child.ox, -anchorW * 0.5f - child.oy, parentContentW, parentContentW};

  if (!parentR.contains(worldX, worldY))
    return parentIdx;

  return resolveCellAtWithSizeCheck(worldX, worldY, parentIdx, parentDim, parentR);
}

int GameView::resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex,
                                          int gridDim, const Rect& r) const {
  const Cell &cell = _model.node(nodeIndex);
  if (cell.type != CellType::GRID) return nodeIndex;

  float childCellSize = r.w / (gridDim + (gridDim - 1) * _gapRatio);

  constexpr float screenWidthNDC = 2.0f;
  if (childCellSize * _zoom < MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT * screenWidthNDC)
    return cell.parent;
  if (childCellSize * _zoom < MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD * screenWidthNDC)
    return nodeIndex;

  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = r.ox + half;
  float startY = r.oy + r.w - half;

  int col = (int)((worldX - startX) / pitch + 0.5f);
  int row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) return nodeIndex;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > half || std::abs(worldY - centerY) > half) return nodeIndex;

  int childIdx = cell.data.grid.firstChild + row * gridDim + col;
  const Cell &child = _model.node(childIdx);

  if (child.type == CellType::GRID) {
    return resolveCellAtWithSizeCheck(worldX, worldY, childIdx,
                                       child.data.grid.gridDimension,
                                       {centerX - half, centerY - half, childCellSize, childCellSize});
  }
  return childIdx;
}

Rect GameView::cellWorldCenter(int targetIdx) const {
  constexpr float anchorW = _anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
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
    int row = offset / gridDim;
    int col = offset % gridDim;
    if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) break;

    Rect child = childCellLayout(nodeIdx, r, row, col);

    if (directChild == targetIdx) {
      return child;
    }

    nodeIdx = directChild;
    r = child;
  }

  return r;
}

void GameView::focusTransform(int targetIdx) {
  Rect r = cellWorldCenter(targetIdx);

  constexpr float maxFocusRatio = 100.0f;
  if (r.w * _zoom > maxFocusRatio * 2.0f)
    return;

  const Cell &cell = _model.node(targetIdx);
  _anchorIndex = targetIdx;
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parent >= 0; i = _model.node(i).parent)
    _anchorDepth++;

  _panX = r.cx() * _zoom + _panX;
  _panY = r.cy() * _aspect * _zoom + _panY;
  _zoom = _zoom * r.w / _anchorWidth;
  if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
}

void GameView::resetView() {
  _anchorIndex = 0;
  const Cell &cell = _model.node(0);
  _anchorSize =
      (cell.type == CellType::GRID) ? cell.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  _zoom = 1.0f;
  _panX = 0.0f;
  _panY = 0.0f;
}

void GameView::focusCenterCell(int winW, int winH) {
  float worldX, worldY;
  screenToWorld(winW / 2, winH / 2, winW, winH, worldX, worldY);
  int idx = resolveCenterCell(worldX, worldY);
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
  int parentIdx = _model.node(_anchorIndex).parent;
  if (parentIdx < 0) return false;

  const Cell &parent = _model.node(parentIdx);
  if (parent.type != CellType::GRID) return false;

  int gridDim = parent.data.grid.gridDimension;
  int firstChild = parent.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int row = offset / gridDim;
  int col = offset % gridDim;

  constexpr float anchorW = _anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  Rect child = childCellLayout(parentIdx, anchorR, row, col);

  float centerX = child.cx();
  float centerY = child.cy();
  float savedZoom = _zoom;

  _anchorIndex = parentIdx;
  _anchorSize = gridDim;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parent >= 0; i = _model.node(i).parent)
    _anchorDepth++;
  _zoom = savedZoom * _anchorWidth / child.w;
  if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
  _panX -= centerX * (savedZoom * _anchorWidth / child.w);
  _panY -= centerY * _aspect * (savedZoom * _anchorWidth / child.w);
  return true;
}

bool GameView::isDescendant(int ancestor, int node) const {
  while (node >= 0) {
    if (node == ancestor) return true;
    node = _model.node(node).parent;
  }
  return false;
}

void GameView::renderAnchor(int anchorIndex, const Rect& r, int depth, int excludeChild) {
  const Cell &cell = _model.node(anchorIndex);
  bool coversScreen = r.ox <= -1.0f && r.oy <= -1.0f &&
                      r.ox + r.w >= 1.0f && r.oy + r.h >= 1.0f;
  if (!coversScreen && cell.parent >= 0) {
    const Cell &parent = _model.node(cell.parent);
    if (parent.type == CellType::GRID) {
      int parentDim = parent.data.grid.gridDimension;
      int firstChild = parent.data.grid.firstChild;
      int offset = anchorIndex - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;

      float k = (float)(parentDim + (parentDim - 1) * _gapRatio);
      float pitchX = r.w * (1 + _gapRatio);
      float pitchY = r.h * (1 + _gapRatio);

      Rect parentR = {
        r.ox - col * pitchX,
        r.oy + r.h - r.h * k + row * pitchY,
        r.w * k,
        r.h * k
      };
      renderAnchor(cell.parent, parentR, depth + 1,
                   excludeChild >= 0 ? excludeChild : anchorIndex);
    }
  }

  if (depth >= MAX_PREVIEW_DEPTH)
    return;
  if (_v + 30 > _verts.data() + _maxVerts)
    return;
  if (r.outsideClip())
    return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize)
    return;

  if (cell.type == CellType::GRID)
    renderGrid(anchorIndex, r, depth, excludeChild);
  else
    renderCell(anchorIndex, r, depth);
}

void GameView::render(int winW, int winH) {
  _aspect = (float)winW / (float)winH;
  size_t maxFloats = (size_t)_model.totalNodes() * 200 + 1024;
  if (maxFloats > _maxVerts) maxFloats = _maxVerts;
  _verts.resize(maxFloats);
  _v = _verts.data();

  glUseProgram(_prog);

  constexpr float anchorW = _anchorWidth;
  float ox = -anchorW * 0.5f * _zoom + _panX;
  float oy = -anchorW * 0.5f * _aspect * _zoom + _panY;
  float cw = anchorW * _zoom;
  float ch = anchorW * _aspect * _zoom;

  renderAnchor(_anchorIndex, {ox, oy, cw, ch}, 0);

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
  _lastVertexCount = totalFloats / 5;

  glDisableVertexAttribArray(_aColorLoc);
  glDisableVertexAttribArray(_aPosLoc);
}

bool GameView::screenToGrid(int pixelX, int pixelY, int winW, int winH, int &row,
                            int &col) const {
  float worldX, worldY;
  screenToWorld(pixelX, pixelY, winW, winH, worldX, worldY);

  int size = _anchorSize;

  float childCellSize = _anchorWidth / (size + (size - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = size * pitch;
  float halfContent = childCellSize * 0.5f;

  float startX = -totalSize * 0.5f + pitch * 0.5f;
  float startY = totalSize * 0.5f - pitch * 0.5f;

  col = (int)((worldX - startX) / pitch + 0.5f);
  row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= size || col < 0 || col >= size)
    return false;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > halfContent ||
      std::abs(worldY - centerY) > halfContent)
    return false;

  return true;
}

void GameView::screenToWorld(int pixelX, int pixelY, int winW, int winH, float &worldX,
                             float &worldY) const {
  float aspect = (float)winW / (float)winH;
  worldX = ((2.0f * pixelX / (float)winW) - 1.0f - _panX) / _zoom;
  worldY = ((1.0f - (2.0f * pixelY / (float)winH)) - _panY) / aspect / _zoom;
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


void GameView::zoom(float factor, float mouseNX, float mouseNY) {
  float oldZoom = _zoom;
  _zoom *= factor;
  if (_zoom < MIN_ZOOM)
    _zoom = MIN_ZOOM;
  float actualFactor = _zoom / oldZoom;
  _panX = mouseNX * (1.0f - actualFactor) + _panX * actualFactor;
  _panY = mouseNY * (1.0f - actualFactor) + _panY * actualFactor;
}

void GameView::startPan(int pixelX, int pixelY) {
  _isPanning = true;
  _lastPanX = pixelX;
  _lastPanY = pixelY;
}

void GameView::continuePan(int pixelX, int pixelY, int winW, int winH) {
  float deltaX = 2.0f * (pixelX - _lastPanX) / (float)winW;
  float deltaY = -2.0f * (pixelY - _lastPanY) / (float)winH;
  _panX += deltaX;
  _panY += deltaY;
  _lastPanX = pixelX;
  _lastPanY = pixelY;
}

void GameView::endPan() { _isPanning = false; }
