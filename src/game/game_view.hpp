#pragma once
#include "../gl.hpp" // IWYU pragma: keep
#include "game_model.hpp"
#include <vector>
#include <stack>

class GameView {
public:
    GameView(GameModel& model);
    ~GameView();

    void initGL();
    void render(int winW, int winH);
    void setDragWorldPos(float wx, float wy) { _dragWX = wx; _dragWY = wy; }
    void setHoveredCell(int row, int col) { _hoverRow = row; _hoverCol = col; }
    void clearHoveredCell() { _hoverRow = -1; _hoverCol = -1; }
    GLuint program() const { return _prog; }

    void zoom(float factor);
    float zoomFactor() const { return _zoom; }
    const float* panPtr() const { return &_panX; }

    bool isPanning() const { return _isPanning; }
    void startPan(int px, int py);
    void continuePan(int px, int py, int winW, int winH);
    void endPan();

    bool screenToGrid(int px, int py, int winW, int winH, int& row, int& col) const;
    void screenToWorld(int px, int py, int winW, int winH, float& wx, float& wy) const;

    float gridToWorldX(int col) const;
    float gridToWorldY(int row) const;

    void focusGrid(int nodeIndex);
    void unfocusGrid();
    bool isFocused() const { return !_focusStack.empty(); }
    int currentRootNode() const;

private:
    void addQuad(float*& v, float cx, float cy, float w, float h, const float color[3]);
    void renderCellItems(float*& v, float cx, float cy, int count, const float color[3]);
    void renderCell(float*& v, int nodeIndex, float cx, float cy, float cellSize, int depth);

    GameModel& _model;

    GLuint _prog = 0, _vbo = 0;
    GLint _aPosLoc = -1, _aColorLoc = -1, _uPosLoc = -1, _uZoomLoc = -1;

    float _zoom = 1.0f;
    float _panX = 0.0f, _panY = 0.0f;
    float _dragWX = 0.0f, _dragWY = 0.0f;
    bool _isPanning = false;
    int _lastPanX = 0, _lastPanY = 0;

    static constexpr float _gridMin = -0.75f;
    static constexpr float _cellSize = 0.30f;
    static constexpr float _gap = 0.02f;
    float _halfRender = (_cellSize - _gap) * 0.5f;

    std::stack<int> _focusStack;
    int _currentGridSize = 0;
    int _hoverRow = -1, _hoverCol = -1;

    static const float _elemColors[GameModel::ELEMS][3];
    static const float _white[3];
    static const float _yellow[3];
    static const float _grey[3];
    static const float _hoverBg[3];
    static const float _gridBg[3];
};
