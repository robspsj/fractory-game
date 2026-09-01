#pragma once
#include "config.hpp"
#include "game_model.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <iostream>
#include <filesystem>

// Skia headers pulled into the header: SkPaint + SkPath are needed as value
// members. We forward-declare SkCanvas and the SVG piece library (PieceLib) so
// the heavy SkCanvas.h / SkSVGDOM.h stay out of this header, keeping TU parse
// times down for fast incremental builds.
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
class SkCanvas;
struct PieceLib;
struct UiLib;
class ParticleSystem;

// Per-frame render-stage timing, accumulated by GameView during renderTo so a
// benchmark can break the frame budget down by what Skia is doing. Reset per
// frame by the driver, read back after renderTo returns.
struct Framebench {
  double glowMs = 0.0;   // item glow radial gradients
  double svgMs  = 0.0;   // per-instance chess-piece SVG rendering
  int    svgN   = 0;     // how many SVG pieces drawn this frame
  void reset() { glowMs = 0.0; svgMs = 0.0; svgN = 0; }
};

struct Rect {
    float ox, oy, w, h;

    float cx() const { return ox + w * 0.5f; }
    float cy() const { return oy + h * 0.5f; }
    float halfW() const { return w * 0.5f; }
    float halfH() const { return h * 0.5f; }
    bool contains(float x, float y) const {
        return x >= ox && x <= ox + w && y >= oy && y <= oy + h;
    }
    bool outsideClip() const {
        return ox + w <= -1.0f || ox >= 1.0f || oy + h <= -1.0f || oy >= 1.0f;
    }
};

class GameView {
public:
    GameView(GameModel& model, const Config& cfg);
    ~GameView();

    void renderTo(SkCanvas* canvas, int winW, int winH);
    // Owner (Game) hands the particle layer to the view for on-scene rendering.
    // Non-owning: the Game owns the ParticleSystem's lifetime.
    void setParticles(ParticleSystem* particles) { _particles = particles; }
    // Load or reload the item‑render CSV. Called automatically from constructor.
    void loadItemConfig(const std::string &path = "assets/items.csv");
    // Debug: render a single icon (piece + glow for `icon`) centered on the given
    // canvas, so alignment of the silhouette vs its glow can be measured directly.
    void debugIcon(SkCanvas* canvas, int w, int h, int icon, float rFrac);
    // Frame-budget internals: reset the per-frame stage timers before renderTo,
    // then read back the Framebench breakdown (glow vs SVG piece time) after.
    void resetFramebench() { _bench.reset(); }
    const Framebench& framebench() const { return _bench; }
    int lastVertexCount() const { return _lastVertexCount; }
    int anchorDepth() const { return _anchorDepth; }
    float lastGenMs() const { return _lastGenMs; }
    float lastDrawMs() const { return _lastDrawMs; }
    // Small for Skia: cached icon paths/paints + the GL surface (GPU path).
    std::size_t allocatedBytes() const;

    void setDragWorldPos(float worldX, float worldY) { _dragWX = worldX; _dragWY = worldY; }
    // Element tint RGB (0..1) for effects/particles (0..ELEMS-1, clamped).
    void elementColor(int id, float out[3]) const {
      int i = id % GameModel::ELEMS;
      if (i < 0) i += GameModel::ELEMS;
      out[0] = _elemColors[i][0]; out[1] = _elemColors[i][1]; out[2] = _elemColors[i][2];
    }
    void setHoveredCell(int row, int col) { _hoverRow = row; _hoverCol = col; }
    void clearHoveredCell() { _hoverRow = -1; _hoverCol = -1; }

    void zoom(float factor, float mouseNX, float mouseNY);
    float zoomFactor() const { return _zoom; }
    const float* panPtr() const { return &_panX; }

    // Load the embedded chess-piece SVGs (src/game/chess_assets.h) as the item
    // icons. Stateless: safe to call once at startup; re-calling re-rasterizes.
    // Native-only: needs modules/svg + system expat; web has no expat and stays
    // on vector symbols.
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
    int resolveLeafCell(float worldX, float worldY) const;

    float gridToWorldX(int col) const;
    float gridToWorldY(int row) const;
    // NDC screen-space centre (and half-extent) of a cell under the CURRENT
    // anchor, computed the SAME way cells are laid out: climb UP from the anchor
    // and UP from the node until their common ancestor, applying the renderer's
    // layout/inverse transforms on both paths. This keeps particles positioned
    // exactly where their cell is drawn under any zoom/focus/pan. Returns false
    // if the node is unreachable. Bounded (tree depth) — no OOB/loops.
    bool cellCenterNDC(int nodeIndex, float &nx, float &ny, float &halfNDC) const;

    void focusCenterCell(int winW, int winH);
    void resetView();

private:
  // Item rendering configuration (loaded from assets/items.csv)
  struct ItemRenderInfo {
    float   color[3] = {1.0f, 1.0f, 1.0f};
    int     icon   = 0;
    std::string svgPath;
  };
  std::unordered_map<int, ItemRenderInfo> _itemRenderInfo;
  std::unordered_map<std::string, std::unique_ptr<class SkSVGDOM>> _svgCache;
  class SkSVGDOM* getSvgDom(const std::string &path);

  int resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim, Rect r) const;
    int resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex, int gridDim, Rect r) const;
    int resolveCenterCell(float worldX, float worldY) const;
    bool unfocusOneLevel();
    bool isDescendant(int ancestor, int node) const;
    Rect childCellLayout(int nodeIndex, Rect r, int row, int col) const;
    Rect cellWorldCenter(int targetIdx) const;
    int variationOf(int nodeIndex) const; // parent grid's visual `variation`
    void focusTransform(int targetIdx);
    void addQuad(Rect r, const float color[3], float alpha = 1.0f);
    // Draw a distinct vector symbol (heart/diamond/star/apple/triangle) with a
    // radial-gradient fill + soft glow, showcasing the rasterizer. cx/cy in NDC.
    void drawIcon(float cx, float cy, int icon, const float color[3],
                  float halfW, float halfH, int instance = 0);
    void renderCellItems(float centerX, float centerY, int count, const float color[3], float scale = 1.0f, int icon = 0);
    // Draw a compact "X{count}" quantity badge at a cell's bottom-right corner.
    void drawCountBadge(const Rect &r, int count);
    void renderEmpty(Rect r, const float bgColor[3] = nullptr);
    void renderItem(Rect r, int itemId, int count, float scale, const float bgColor[3] = nullptr, int variation = 0);
    void renderGrid(int nodeIndex, Rect r, int depth, int excludeChild = -1);
    void renderCell(int nodeIndex, Rect r, int depth);
    void renderAnchor(int anchorIndex, Rect r, int depth, int excludeChild = -1);
    static uint32_t rgba(const float color[3], float alpha);

    // Cached per-element icon style. Shapes+shaders live in UNIT space centred
    // on the origin with radius ~1; drawIcon places them with a translate+scale
    // so we construct each symbol's path/gradient exactly once (not per frame).
    struct IconStyle {
        SkPath symbol;    // unit-space symbol shape (filled)
        SkPaint symPaint; // radial-gradient fill (unit radius)
        SkPaint glowPaint; // radial-gradient glow disc (unit radius 2.2)
        SkPath stem;      // apple stem (empty for other icons)
        SkPaint stemPaint; // stroke
    };
    std::array<IconStyle, GameModel::ELEMS> _icons;
    void buildIcon(int icon, const float color[3]);
    #if !defined(__EMSCRIPTEN__)
    // Chess-piece SVG library: parses the embedded SVGs once and lazily
    // re-rasterizes each piece at the icon's current on-screen pixel size, so
    // items stay crisp (vector-clean) at any zoom — never a fixed upscaled PNG.
    PieceLib* _pieces = nullptr;
    // Rounded-color-square UI tiles (grid bg + inner cells), per-size families.
    UiLib* _ui = nullptr;
#endif
    void drawBackground(int winW, int winH, double t);

    GameModel& _model;
    Framebench _bench;   // accumulate glow/svg render-stage times (for benchmarks)

    SkCanvas* _canvas = nullptr;
    SkPaint _quadPaint;   // scratch fill for the fast rect path
    int _targetW = 0, _targetH = 0;

    bool _showAnchor = false;
    uint64_t _dragAnimStartMs = 0;
    bool _dragWasActive = false;
    static constexpr double _dragAnimDuration = 0.15;

    float _zoom = 1.0f;
    float _panX = 0.0f, _panY = 0.0f;
    float _aspect = 1.0f;
    // Reference zoom that fits the whole board to the SHORTER window side
    // (letterboxed, squares stay square); re-fit when the window resizes.
    float _fitZoom = 1.0f;
    int _lastWinW = 0, _lastWinH = 0;
    float _dragWX = 0.0f, _dragWY = 0.0f;
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
    int _hoverRow = -1, _hoverCol = -1;

    static const float _elemColors[GameModel::ELEMS][3];
    static const float _white[3];
    static const float _yellow[3];
    static const float _grey[3];
    static const float _gridBg[3];
    static const float _stationColor[3];
    static const float _reservedInput[3];
    static const float _reservedOutput[3];
    static const float _reservedBuffer[3];
};