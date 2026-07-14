#pragma once
#include "../gl.hpp" // IWYU pragma: keep
#include "game_model.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstddef>
#include <vector>

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
    GameView(GameModel& model);
    ~GameView();

    void initGL();
    void render(int winW, int winH);
    void setDragWorldPos(float worldX, float worldY) { _dragWX = worldX; _dragWY = worldY; }
    void setHoveredCell(int row, int col) { _hoverRow = row; _hoverCol = col; }
    void clearHoveredCell() { _hoverRow = -1; _hoverCol = -1; }
    GLuint program() const { return _prog; }

    void zoom(float factor, float mouseNX, float mouseNY);
    float zoomFactor() const { return _zoom; }
    const float* panPtr() const { return &_panX; }

    bool isPanning() const { return _isPanning; }
    void startPan(int pixelX, int pixelY);
    void continuePan(int pixelX, int pixelY, int winW, int winH);
    void endPan();

    bool screenToGrid(int pixelX, int pixelY, int winW, int winH, int& row, int& col) const;
    void screenToWorld(int pixelX, int pixelY, int winW, int winH, float& worldX, float& worldY) const;
    int resolveLeafCell(float worldX, float worldY) const;

    float gridToWorldX(int col) const;
    float gridToWorldY(int row) const;

    void focusCenterCell(int winW, int winH);
    void resetView();

private:
    int resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim, const Rect& r) const;
    int resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex, int gridDim, const Rect& r) const;
    int resolveCenterCell(float worldX, float worldY) const;
    bool unfocusOneLevel();
    bool isDescendant(int ancestor, int node) const;
    Rect childCellLayout(int nodeIndex, const Rect& r, int row, int col) const;
    Rect cellWorldCenter(int targetIdx) const;
    void focusTransform(int targetIdx);
    void addQuad(const Rect& r, const float color[3]);
    void renderCellItems(float centerX, float centerY, int count, const float color[3], float scale = 1.0f);
    void renderEmpty(const Rect& r, const float bgColor[3] = nullptr);
    void renderItem(const Rect& r, int itemId, int count, float scale, const float bgColor[3] = nullptr);
    void renderGrid(int nodeIndex, const Rect& r, int depth, int excludeChild = -1);
    void renderCell(int nodeIndex, const Rect& r, int depth);
    void renderAnchor(int anchorIndex, const Rect& r, int depth, int excludeChild = -1);

    GameModel& _model;

    Uint64 _dragAnimStartTime = 0;
    bool _dragWasActive = false;
    static constexpr double _dragAnimDuration = 0.15;

    GLuint _prog = 0, _vbo = 0;
    GLint _aPosLoc = -1, _aColorLoc = -1;

    float _zoom = 1.0f;
    float _panX = 0.0f, _panY = 0.0f;
    float _aspect = 1.0f;
    float _dragWX = 0.0f, _dragWY = 0.0f;
    bool _isPanning = false;
    int _lastPanX = 0, _lastPanY = 0;

    std::vector<float> _verts;
    float* _v = nullptr;

    static constexpr float _gridMin = -0.75f;
    static constexpr float _cellSize = 0.30f;
    static constexpr float _gapRatio = 0.0714f;
    static constexpr float _anchorWidth = 1.5f;
    static constexpr size_t _maxVerts = 131072;

    int _anchorIndex = 0;
    int _anchorSize = 0;
    int _hoverRow = -1, _hoverCol = -1;

    static const float _elemColors[GameModel::ELEMS][3];
    static const float _white[3];
    static const float _yellow[3];
    static const float _grey[3];
    static const float _gridBg[3];
};
