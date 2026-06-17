#include "game_view.hpp"
#include "../shader.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f},
    {0.90f, 0.30f, 0.30f},
    {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f},
    {0.80f, 0.30f, 0.85f},
};

const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_gridBg[3] = {0.8f, 0.2f, 0.2f};

GameView::GameView(GameModel& model) : _model(model) {}

GameView::~GameView() {
    if (_prog) glDeleteProgram(_prog);
    if (_vbo) glDeleteBuffers(1, &_vbo);
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

void GameView::addQuad(float*& v, float cx, float cy, float w, float h, const float color[3]) {
    for (int i = 0; i < 6; i++) {
        float dx = (i == 1 || i == 3 || i == 4) ? w : -w;
        float dy = (i == 0 || i == 1 || i == 4) ? -h : h;
        v[0] = cx + dx; v[1] = cy + dy;
        v[2] = color[0]; v[3] = color[1]; v[4] = color[2];
        v += 5;
    }
}

void GameView::renderCellItems(float*& v, float cx, float cy, int count, const float color[3]) {
    if (count <= 0) return;
    float itemDotSize = 0.05f;
    float spacing = itemDotSize * 2.5f;
    switch (count) {
        case 1: addQuad(v, cx, cy, itemDotSize, itemDotSize, color); break;
        case 2: addQuad(v, cx - spacing * 0.5f, cy, itemDotSize, itemDotSize, color); addQuad(v, cx + spacing * 0.5f, cy, itemDotSize, itemDotSize, color); break;
        case 3: addQuad(v, cx, cy - spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx - spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx + spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize, color); break;
        case 4: addQuad(v, cx - spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx + spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx - spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx + spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize, color); break;
        case 5: addQuad(v, cx, cy, itemDotSize, itemDotSize, color); addQuad(v, cx - spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx + spacing * 0.5f, cy - spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx - spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize, color); addQuad(v, cx + spacing * 0.5f, cy + spacing * 0.5f, itemDotSize, itemDotSize, color); break;
        default: addQuad(v, cx, cy, itemDotSize, itemDotSize, color); break;
    }
}

void GameView::renderCell(float*& v, int nodeIndex, float cx, float cy, float cellSize, int depth) {
    const auto& cell = _model.node(nodeIndex);

    if (cell.type == CellType::ITEM) {
        const float* col = _elemColors[cell.data.item.id];
        renderCellItems(v, cx, cy, cell.data.item.count, col);
    } else if (cell.type == CellType::GRID) {
        static constexpr int MAX_PREVIEW_DEPTH = 3;

        int firstChild = cell.data.grid.firstChild;
        int size = cell.data.grid.size;

        float pitch = cellSize / size;
        float halfContent = (pitch - _gap) * 0.5f;
        if (halfContent < 0.001f) halfContent = pitch * 0.45f;

        float startX = cx - cellSize * 0.5f + pitch * 0.5f;
        float startY = cy - cellSize * 0.5f + pitch * 0.5f;

        for (int r = 0; r < size; r++) {
            for (int c = 0; c < size; c++) {
                float childCx = startX + c * pitch;
                float childCy = startY + r * pitch;

                int childIndex = firstChild + r * size + c;
                const auto& childNode = _model.node(childIndex);

                if (childNode.type == CellType::ITEM) {
                    addQuad(v, childCx, childCy, halfContent, halfContent, _grey);
                    const float* col = _elemColors[childNode.data.item.id];
                    renderCellItems(v, childCx, childCy, childNode.data.item.count, col);
                } else if (childNode.type == CellType::EMPTY) {
                    addQuad(v, childCx, childCy, halfContent, halfContent, _grey);
                } else if (childNode.type == CellType::GRID) {
                    if (depth + 1 < MAX_PREVIEW_DEPTH && pitch > _gap * 3.0f) {
                        renderCell(v, childIndex, childCx, childCy, pitch - _gap, depth + 1);
                    } else {
                        addQuad(v, childCx, childCy, halfContent * 0.5f, halfContent * 0.5f, _gridBg);
                    }
                }
            }
        }
    }
}

void GameView::render(int winW, int winH) {
    float aspect = (float)winW / (float)winH;
    static float verts[1024 * 1024];
    float* v = verts;

    int rootNodeIndex = isFocused() ? _focusStack.top() : 0;
    const auto& rootNode = _model.node(rootNodeIndex);
    _currentGridSize = (rootNode.type == CellType::GRID) ? rootNode.data.grid.size : GameModel::GRID;

    glUseProgram(_prog);
    glUniform2f(_uPosLoc, _panX, _panY);
    glUniform1f(_uZoomLoc, _zoom);
    glUniform1f(glGetUniformLocation(_prog, "uAspect"), aspect);

    float totalGridSize = _currentGridSize * _cellSize;
    renderCell(v, rootNodeIndex, 0.0f, 0.0f, totalGridSize, 0);

    if (_model.hasDrag()) {
        int dragId = _model.dragItemId();
        int dragAmount = _model.dragAmount();
        const float* col = _elemColors[dragId];
        renderCellItems(v, _dragWX, _dragWY, dragAmount, col);
    }

    int totalFloats = (int)(v - verts);
    if (totalFloats == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, totalFloats * sizeof(float), verts, GL_STREAM_DRAW);

    glVertexAttribPointer(_aPosLoc, 2, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float), 0);
    glEnableVertexAttribArray(_aPosLoc);
    glVertexAttribPointer(_aColorLoc, 3, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(_aColorLoc);

    glDrawArrays(GL_TRIANGLES, 0, totalFloats / 5);

    glDisableVertexAttribArray(_aColorLoc);
    glDisableVertexAttribArray(_aPosLoc);
}

bool GameView::screenToGrid(int px, int py, int winW, int winH, int& row, int& col) const {
    float wx, wy;
    screenToWorld(px, py, winW, winH, wx, wy);

    int rootIdx = isFocused() ? _focusStack.top() : 0;
    const auto& root = _model.node(rootIdx);
    int size = (root.type == CellType::GRID) ? root.data.grid.size : GameModel::GRID;

    float pitch = _cellSize;
    float totalSize = size * pitch;
    float halfContent = (pitch - _gap) * 0.5f;
    if (halfContent < 0.001f) halfContent = pitch * 0.45f;

    float startX = -totalSize * 0.5f + pitch * 0.5f;
    float startY = -totalSize * 0.5f + pitch * 0.5f;

    col = (int)((wx - startX) / pitch + 0.5f);
    row = (int)((wy - startY) / pitch + 0.5f);

    if (row < 0 || row >= size || col < 0 || col >= size) return false;

    float centerX = startX + col * pitch;
    float centerY = startY + row * pitch;
    if (std::abs(wx - centerX) > halfContent || std::abs(wy - centerY) > halfContent)
        return false;

    return true;
}

void GameView::screenToWorld(int px, int py, int winW, int winH, float& wx, float& wy) const {
    float aspect = (float)winW / (float)winH;
    wx = ((2.0f * px / (float)winW) - 1.0f - _panX) / _zoom;
    wy = ((1.0f - (2.0f * py / (float)winH)) - _panY) / aspect / _zoom;
}

float GameView::gridToWorldX(int col) const {
    float pitch = _cellSize;
    float totalSize = _currentGridSize * pitch;
    return -totalSize * 0.5f + pitch * 0.5f + col * pitch;
}

float GameView::gridToWorldY(int row) const {
    float pitch = _cellSize;
    float totalSize = _currentGridSize * pitch;
    return -totalSize * 0.5f + pitch * 0.5f + row * pitch;
}

void GameView::focusGrid(int nodeIndex) {
    if (nodeIndex >= 0 && _model.node(nodeIndex).type == CellType::GRID) {
        _focusStack.push(nodeIndex);
    }
}

void GameView::unfocusGrid() {
    if (!_focusStack.empty()) {
        _focusStack.pop();
    }
}

int GameView::currentRootNode() const {
    return _focusStack.empty() ? 0 : _focusStack.top();
}

void GameView::zoom(float factor) {
    _zoom *= factor;
    if (_zoom < 0.1f) _zoom = 0.1f;
    if (_zoom > 10.0f) _zoom = 10.0f;
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

void GameView::endPan() {
    _isPanning = false;
}
