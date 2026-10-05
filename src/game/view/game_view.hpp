#pragma once
#include "../config.hpp"
#include "../model/game_model.hpp"
#include "../model/cell.hpp"
#include "../framebench.hpp"
#include "types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <iostream>

#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
class SkCanvas;
struct PieceLib;
struct UiLib;
class ParticleSystem;
struct PerPathSvg;

// Forward helpers (friends)
class LayoutEngine;
class CoordinateMapper;
class PickingEngine;
class CameraController;
class AnchorManager;
class IconRenderer;
class CellRenderer;
class BadgeRenderer;
class BackgroundRenderer;
class AssetManager;

class GameView {
 public:
    GameView(GameModel& model, const Config& cfg);
    ~GameView();

    void renderTo(SkCanvas* canvas, int winW, int winH);
    void setParticles(ParticleSystem* particles) { _particles = particles; }
    void loadItemConfig(const std::string &path = "assets/items.csv");
    void debugIcon(SkCanvas* canvas, int w, int h, int icon, float rFrac);
    // Draw one item icon at pixel (cx,cy) with half-extent halfPx — used by the
    // recipe-book overlay. Primes canvas/target size the way the renderers do
    // (drawIcon works in NDC and resets the matrix), then restores the canvas.
    void drawItemIcon(SkCanvas *canvas, float cx, float cy, float halfPx, int itemId);
    // Item display name from items.csv ("" when the id has no row) — used by the
    // recipe book so entries read "wood" instead of a bare id.
    const std::string &itemName(int itemId) const;
    void resetFramebench() { _bench.reset(); }
    const Framebench& framebench() const { return _bench; }
    int lastVertexCount() const { return _lastVertexCount; }
    int anchorDepth() const { return _anchorDepth; }
    float lastGenMs() const { return _lastGenMs; }
    float lastDrawMs() const { return _lastDrawMs; }
    std::size_t allocatedBytes() const;

    void setDragScreenNdc(float ndcX, float ndcY) { _dragNX = ndcX; _dragNY = ndcY; }
    void elementColor(int id, float out[3]) const {
      int i = id % GameModel::ELEMS;
      if (i < 0) i += GameModel::ELEMS;
      out[0] = _elemColors[i][0]; out[1] = _elemColors[i][1]; out[2] = _elemColors[i][2];
    }
    void setHoveredCellNode(int nodeIdx) { _hoverCellNode = nodeIdx; }
    void clearHoveredCell() { _hoverCellNode = -1; }
    int hoveredCellNode() const { return _hoverCellNode; }

    void zoom(float factor, float mouseNX, float mouseNY);
    float zoomFactor() const { return _zoom; }
    const float* panPtr() const { return &_panX; }

#if !defined(__EMSCRIPTEN__)
    void loadChessAssets();
    void loadUiAssets();
#endif

    bool isPanning() const { return _isPanning; }
    void startPan(int pixelX, int pixelY);
    void continuePan(int pixelX, int pixelY, int winW, int winH);
    void endPan();

    bool screenToGrid(int pixelX, int pixelY, int winW, int winH, int& row, int& col) const;
    void screenToWorld(int pixelX, int pixelY, int winW, int winH, float& worldX, float& worldY) const;
    void screenToNdc(int pixelX, int pixelY, int winW, int winH, float& nx, float& ny) const;
    int resolveLeafCell(float worldX, float worldY) const;
    // Gap pick (docs/reactions.md): true when the point is aimed at the space
    // BETWEEN cells rather than at a cell itself — the outer band of a cell, the
    // band being wider than the visible gutter so it can be hit without pixel
    // hunting. cellIdx is then that nearest cell and `side` names the edge (2
    // cells) or corner (4 cells) the click points at. False = the point is in
    // the middle of a cell, i.e. a cell action, not a reaction.
    bool resolveGap(float worldX, float worldY, int &cellIdx,
                    GameModel::GapSide &side) const;

    float gridToWorldX(int col) const;
    float gridToWorldY(int row) const;
    bool cellCenterNDC(int nodeIndex, float &nx, float &ny, float &halfNDC) const;
    bool isVisible(int idx) const;
    bool getNdc(int idx, float &x, float &y, float &half) const;

    void focusCenterCell(int winW, int winH);
    void resetView();

 private:
  // Item rendering configuration — data-driven per-item.
  // sway == PerPath: each <path> in the SVG sways with its own frequency/phase
  // (lilypad leaves etc. move independently). Static: whole icon rotates as one.
  enum class SwayMode { Static, PerPath };
  struct ItemRenderInfo {
    float   color[3] = {1.0f, 1.0f, 1.0f};
    int     icon   = 0;
    std::string name;   // items.csv column 2 (used by the recipe book)
    std::string svgPath;
    SwayMode sway = SwayMode::Static;
  };
  std::unordered_map<int, ItemRenderInfo> _itemRenderInfo;
#if !defined(__EMSCRIPTEN__)
  std::unordered_map<std::string, std::unique_ptr<class SkSVGDOM>> _svgCache;
#endif
  std::unordered_map<std::string, std::unique_ptr<class PerPathSvg>> _perPathCache;
#if !defined(__EMSCRIPTEN__)
  class SkSVGDOM* getSvgDom(const std::string &path);
#endif
  PerPathSvg* getPerPathSvg(const std::string &path);

  int resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim, Rect r) const;
    int resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex, int gridDim, Rect r) const;
    int resolveCenterCell(float worldX, float worldY) const;
    bool unfocusOneLevel();
    bool isDescendant(int ancestor, int node) const;
    Rect childCellLayout(int nodeIndex, Rect r, int row, int col) const;
    Rect cellWorldCenter(int targetIdx) const;
    int variationOf(int nodeIndex) const;
    void focusTransform(int targetIdx);
    void addQuad(Rect r, const float color[3], float alpha = 1.0f);
    void drawIcon(float cx, float cy, int icon, const float color[3],
                  float halfW, float halfH, int instance = 0, int nodeIndex = -1);
    void renderCellItems(float centerX, float centerY, int count, const float color[3], float scale = 1.0f, int icon = 0, int nodeIndex = -1);
    void drawCountBadge(const Rect &r, int count);
    void renderEmpty(Rect r, const float bgColor[3] = nullptr);
    void renderItem(Rect r, int itemId, int count, float scale, const float bgColor[3] = nullptr, int variation = 0, int nodeIndex = -1);
    void renderGrid(int nodeIndex, Rect r, int depth, int excludeChild = -1);
    void renderCell(int nodeIndex, Rect r, int depth);
    void renderAnchor(int anchorIndex, Rect r, int depth, int excludeChild = -1);
    static uint32_t rgba(const float color[3], float alpha);

    struct IconStyle {
        SkPath symbol;
        SkPaint symPaint;
        SkPaint glowPaint;
        SkPath stem;
        SkPaint stemPaint;
    };
    std::array<IconStyle, GameModel::ELEMS> _icons;
    void buildIcon(int icon, const float color[3]);
#if !defined(__EMSCRIPTEN__)
    PieceLib* _pieces = nullptr;
    UiLib* _ui = nullptr;
#endif
    void drawBackground(int winW, int winH, double t);

    GameModel& _model;
    Framebench _bench;

    SkCanvas* _canvas = nullptr;
    SkPaint _quadPaint;
    int _targetW = 0, _targetH = 0;

    bool _showAnchor = false;
    uint64_t _dragAnimStartMs = 0;
    bool _dragWasActive = false;
    static constexpr double _dragAnimDuration = 0.15;

    float _zoom = 1.0f;
    float _panX = 0.0f, _panY = 0.0f;
    float _aspect = 1.0f;
    float _fitZoom = 1.0f;
    int _lastWinW = 0, _lastWinH = 0;
    float _dragNX = 0.0f, _dragNY = 0.0f;
    bool _isPanning = false;
    ParticleSystem* _particles = nullptr;
    int _lastPanX = 0, _lastPanY = 0;

    static constexpr float _gridMin = -0.75f;
    static constexpr float _cellSize = 0.30f;
    static constexpr float _gapRatio = 0.0714f;
    static constexpr float _anchorWidth = 1.5f;

    float _lastGenMs = 0.0f;
    float _lastDrawMs = 0.0f;
    int _lastVertexCount = 0;

    int _anchorIndex = 0;
    int _anchorSize = 0;
    int _anchorDepth = 0;
    int _hoverCellNode = -1;

    // Optimized visible + NDC cache for particles: bit i == node i was rendered
    // this frame (on-screen, not culled). NDC cache (x,y,half) is filled once
    // per visible cell per frame during the anchor walk, then particles look up
    // without any tree walk. Both are view state only, cleaned every frame.
    std::vector<uint32_t> _visibleBits;
    std::unordered_map<int, std::array<float,3>> _ndcCache;
    void ensureVisibleSize();
    void clearVisible();
    void setVisible(int idx);
    void setNdc(int idx, float x, float y, float half);

    static const float _elemColors[GameModel::ELEMS][3];
    static const float _white[3];
    static const float _yellow[3];
    static const float _grey[3];
    static const float _gridBg[3];
    static const float _stationColor[3];
    static const float _reservedInput[3];
    static const float _reservedOutput[3];
    static const float _reservedBuffer[3];

  friend class LayoutEngine;
  friend class CoordinateMapper;
  friend class PickingEngine;
  friend class CameraController;
  friend class AnchorManager;
  friend class IconRenderer;
  friend class CellRenderer;
  friend class BadgeRenderer;
  friend class BackgroundRenderer;
  friend class AssetManager;
  friend class RecipeBook;
};
