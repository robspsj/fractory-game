#include "game_view.hpp"
#include "layout_engine.hpp"
#include "coordinate_mapper.hpp"
#include "picking_engine.hpp"
#include "camera_controller.hpp"
#include "anchor_manager.hpp"
#include "icon_renderer.hpp"
#include "cell_renderer.hpp"
#include "badge_renderer.hpp"
#include "background_renderer.hpp"
#include "asset_manager.hpp"
#include "piece_library.hpp"
#include "ui_library.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkM44.h"
#include "include/core/SkImage.h"
#include "../particle.hpp"

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f}, {0.90f, 0.30f, 0.30f}, {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f}, {0.80f, 0.30f, 0.85f},
};
const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_gridBg[3] = {0.25f, 0.40f, 0.60f};
const float GameView::_stationColor[3] = {0.6f, 0.4f, 0.2f};
const float GameView::_reservedInput[3] = {0.2f, 0.6f, 0.3f};
const float GameView::_reservedOutput[3] = {0.6f, 0.2f, 0.2f};
const float GameView::_reservedBuffer[3] = {0.4f, 0.4f, 0.4f};

static uint64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

GameView::GameView(GameModel &model, const Config &cfg) : _model(model), _showAnchor(cfg.showAnchor) {
  AssetManager::loadItemConfig(*this);
  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize = (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId) _anchorDepth++;
  for (int i = 0; i < GameModel::ELEMS; i++) IconRenderer::buildIcon(*this, i, _elemColors[i]);
}

GameView::~GameView() {
#if !defined(__EMSCRIPTEN__)
  delete _pieces;
  delete _ui;
#endif
}

void GameView::loadItemConfig(const std::string &path) { AssetManager::loadItemConfig(*this, path); }
SkSVGDOM* GameView::getSvgDom(const std::string &path) { return AssetManager::getSvgDom(*this, path); }
#if !defined(__EMSCRIPTEN__)
void GameView::loadChessAssets() { AssetManager::loadChessAssets(*this); }
void GameView::loadUiAssets() { AssetManager::loadUiAssets(*this); }
#endif

void GameView::debugIcon(SkCanvas* canvas, int w, int h, int icon, float rFrac) {
  _canvas = canvas; _targetW = w; _targetH = h; _aspect = (float)w / (float)h;
  const int ti = icon % GameModel::ELEMS;
  IconRenderer::drawIcon(*this, 0.0f, 0.0f, ti, _elemColors[ti], rFrac, rFrac);
  _canvas = nullptr;
}

std::size_t GameView::allocatedBytes() const {
  std::size_t bytes = 0;
  bytes += _icons.size() * sizeof(IconStyle);
  bytes += _visibleBits.size() * sizeof(uint32_t);
  bytes += _ndcCache.size() * (sizeof(int) + sizeof(std::array<float,3>));
  return bytes;
}

void GameView::ensureVisibleSize() {
  size_t need = (_model.totalNodes() + 31) / 32;
  if (_visibleBits.size() != need) _visibleBits.assign(need, 0);
  else std::fill(_visibleBits.begin(), _visibleBits.end(), 0);
  _ndcCache.clear();
  _ndcCache.reserve(4096);
}
void GameView::clearVisible() {
  std::fill(_visibleBits.begin(), _visibleBits.end(), 0);
  _ndcCache.clear();
}
void GameView::setVisible(int idx) {
  if (idx < 0) return;
  size_t w = (size_t)idx >> 5;
  if (w >= _visibleBits.size()) return;
  _visibleBits[w] |= 1u << (idx & 31);
}
bool GameView::isVisible(int idx) const {
  if (idx < 0) return false;
  size_t w = (size_t)idx >> 5;
  if (w >= _visibleBits.size()) return false;
  return (_visibleBits[w] & (1u << (idx & 31))) != 0;
}
void GameView::setNdc(int idx, float x, float y, float half) {
  if (idx < 0) return;
  _ndcCache[idx] = {x, y, half};
}
bool GameView::getNdc(int idx, float &x, float &y, float &half) const {
  auto it = _ndcCache.find(idx);
  if (it == _ndcCache.end()) return false;
  x = it->second[0]; y = it->second[1]; half = it->second[2];
  return true;
}

// Delegating wrappers
Rect GameView::childCellLayout(int nodeIndex, Rect r, int row, int col) const { return LayoutEngine::childCellLayout(*this, nodeIndex, r, row, col); }
Rect GameView::cellWorldCenter(int targetIdx) const { return LayoutEngine::cellWorldCenter(*this, targetIdx); }
bool GameView::cellCenterNDC(int nodeIndex, float &nx, float &ny, float &halfNDC) const { return LayoutEngine::cellCenterNDC(*this, nodeIndex, nx, ny, halfNDC); }
int GameView::variationOf(int nodeIndex) const { return LayoutEngine::variationOf(*this, nodeIndex); }
bool GameView::isDescendant(int ancestor, int node) const { return LayoutEngine::isDescendant(*this, ancestor, node); }

void GameView::screenToWorld(int pixelX, int pixelY, int winW, int winH, float &worldX, float &worldY) const { CoordinateMapper::screenToWorld(*this, pixelX, pixelY, winW, winH, worldX, worldY); }
void GameView::screenToNdc(int pixelX, int pixelY, int winW, int winH, float &nx, float &ny) const { CoordinateMapper::screenToNdc(*this, pixelX, pixelY, winW, winH, nx, ny); }
bool GameView::screenToGrid(int pixelX, int pixelY, int winW, int winH, int &row, int &col) const { return CoordinateMapper::screenToGrid(*this, pixelX, pixelY, winW, winH, row, col); }
float GameView::gridToWorldX(int col) const { return CoordinateMapper::gridToWorldX(*this, col); }
float GameView::gridToWorldY(int row) const { return CoordinateMapper::gridToWorldY(*this, row); }

int GameView::resolveLeafCell(float worldX, float worldY) const { return PickingEngine::resolveLeafCell(*this, worldX, worldY); }
int GameView::resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim, Rect r) const { return PickingEngine::resolveCellAt(*this, worldX, worldY, nodeIndex, gridDim, r); }
int GameView::resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex, int gridDim, Rect r) const { return PickingEngine::resolveCellAtWithSizeCheck(*this, worldX, worldY, nodeIndex, gridDim, r); }
int GameView::resolveCenterCell(float worldX, float worldY) const { return PickingEngine::resolveCenterCell(*this, worldX, worldY); }

void GameView::zoom(float factor, float mouseNX, float mouseNY) { CameraController::zoom(*this, factor, mouseNX, mouseNY); }
void GameView::startPan(int pixelX, int pixelY) { CameraController::startPan(*this, pixelX, pixelY); }
void GameView::continuePan(int pixelX, int pixelY, int winW, int winH) { CameraController::continuePan(*this, pixelX, pixelY, winW, winH); }
void GameView::endPan() { CameraController::endPan(*this); }

void GameView::focusTransform(int targetIdx) { AnchorManager::focusTransform(*this, targetIdx); }
void GameView::resetView() { AnchorManager::resetView(*this); }
void GameView::focusCenterCell(int winW, int winH) { AnchorManager::focusCenterCell(*this, winW, winH); }
bool GameView::unfocusOneLevel() { return AnchorManager::unfocusOneLevel(*this); }

void GameView::addQuad(Rect r, const float color[3], float alpha) { CellRenderer::addQuad(*this, r, color, alpha); }
void GameView::drawIcon(float cx, float cy, int icon, const float color[3], float halfW, float halfH, int instance) { IconRenderer::drawIcon(*this, cx, cy, icon, color, halfW, halfH, instance); }
void GameView::renderCellItems(float centerX, float centerY, int count, const float color[3], float scale, int icon) { IconRenderer::renderCellItems(*this, centerX, centerY, count, color, scale, icon); }
void GameView::drawCountBadge(const Rect &r, int count) { BadgeRenderer::drawCountBadge(*this, r, count); }
void GameView::renderEmpty(Rect r, const float bgColor[3]) { CellRenderer::renderEmpty(*this, r, bgColor); }
void GameView::renderItem(Rect r, int itemId, int count, float scale, const float bgColor[3], int variation) { CellRenderer::renderItem(*this, r, itemId, count, scale, bgColor, variation); }
void GameView::renderGrid(int nodeIndex, Rect r, int depth, int excludeChild) { CellRenderer::renderGrid(*this, nodeIndex, r, depth, excludeChild); }
void GameView::renderCell(int nodeIndex, Rect r, int depth) { CellRenderer::renderCell(*this, nodeIndex, r, depth); }
void GameView::renderAnchor(int anchorIndex, Rect r, int depth, int excludeChild) { CellRenderer::renderAnchor(*this, anchorIndex, r, depth, excludeChild); }
uint32_t GameView::rgba(const float color[3], float alpha) { return IconRenderer::rgba(color, alpha); }
void GameView::buildIcon(int icon, const float color[3]) { IconRenderer::buildIcon(*this, icon, color); }
void GameView::drawBackground(int winW, int winH, double t) {
#if !defined(__EMSCRIPTEN__)
  BackgroundRenderer::drawBackground(*this, winW, winH, t);
#else
  (void)winW; (void)winH; (void)t;
#endif
}

void GameView::renderTo(SkCanvas *canvas, int winW, int winH) {
  if (!canvas) return;
  _canvas = canvas;
  _bench.reset();
  _targetW = winW;
  _targetH = winH;
  _aspect = (float)winW / (float)winH;
  if (winW != _lastWinW || winH != _lastWinH) {
    int ow = _lastWinW, oh = _lastWinH;
    _lastWinW = winW; _lastWinH = winH;
    const float newMin = (float)std::min(winW, winH);
    const float newFit = newMin / (float)winW;
    if (ow == 0 || oh == 0) { _zoom = newFit; _panX = 0.0f; _panY = 0.0f; }
    else { const float oldMin = (float)std::min(ow, oh); const float zr = (newMin / (float)winW) / (oldMin / (float)ow); _zoom *= zr; _panX *= zr; _panY *= (newMin / (float)winH) / (oldMin / (float)oh); if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM; if (_zoom > MAX_ZOOM) _zoom = MAX_ZOOM; }
    _fitZoom = newFit;
  }
  _lastVertexCount = 0;
  ensureVisibleSize();
  clearVisible();
  SkMatrix ndc; ndc.setScaleTranslate((SkScalar)(winW * 0.5), (SkScalar)(-winH * 0.5), (SkScalar)(winW * 0.5), (SkScalar)(winH * 0.5));
  canvas->setMatrix(SkM44(ndc));
  uint64_t t0 = nowMs();
  canvas->clear(SkColorSetARGB(0xFF, 0x1A, 0x1A, 0x26));
#if !defined(__EMSCRIPTEN__)
  {
    auto nowUs = []{ return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
    static int64_t base = (int64_t)nowUs();
    double t = (double)((int64_t)nowUs() - base) / 1e6;
    BackgroundRenderer::drawBackground(*this, winW, winH, t);
  }
#endif
  constexpr float anchorW = _anchorWidth;
  float ox = -anchorW * 0.5f * _zoom + _panX;
  float oy = -anchorW * 0.5f * _aspect * _zoom + _panY;
  float cw = anchorW * _zoom;
  float ch = anchorW * _aspect * _zoom;
  {
    uint64_t a0 = nowMs();
    CellRenderer::renderAnchor(*this, _anchorIndex, {ox, oy, cw, ch}, 0);
    uint64_t a1 = nowMs();
    double anchorMs = (double)(a1 - a0) / 1000.0;
    double sub = _bench.svgMs + _bench.glowMs + _bench.textMs;
    _bench.cellsMs = anchorMs > sub ? anchorMs - sub : 0.0;
  }
  if (_model.hasDrag()) {
    int dragId = _model.dragItemId();
    int dragAmount = _model.dragAmount();
    if (!_dragWasActive) _dragAnimStartMs = nowMs();
    double elapsed = (double)(nowMs() - _dragAnimStartMs) / 1000.0;
    double t = elapsed / _dragAnimDuration; if (t >= 1.0) t = 1.0;
    float eased = (float)(t * (2.0 - t));
    float scale = (0.5f + 0.5f * eased) / _zoom;
    float sx = _dragNX; float sy = _dragNY;
    const float *elemCol = (dragId >= 0 && dragId < GameModel::ELEMS) ? _elemColors[dragId] : _white;
    int dragIcon = dragId;
    IconRenderer::renderCellItems(*this, sx, sy, dragAmount, elemCol, scale, dragIcon);
  }
  _dragWasActive = _model.hasDrag();
  if (_particles) _particles->draw(_canvas, winW, winH, [this](int n, float &x, float &y, float &h){
    return getNdc(n, x, y, h);
  });
  uint64_t t1 = nowMs();
  _lastGenMs = (float)(t1 - t0);
  _lastDrawMs = (float)(nowMs() - t1);
  _canvas = nullptr;
}
