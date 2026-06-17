#include "game_view.hpp"
#include "../shader.hpp"

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

    switch (count) {
        case 1: {
            float size = 0.05f;
            addQuad(v, cx, cy, size, size, color);
            break;
        }
        case 2: {
            float size = 0.04f;
            addQuad(v, cx - 0.05f, cy, size, size, color);
            addQuad(v, cx + 0.05f, cy, size, size, color);
            break;
        }
        case 3: {
            float size = 0.035f;
            addQuad(v, cx - 0.05f, cy - 0.04f, size, size, color);
            addQuad(v, cx + 0.05f, cy - 0.04f, size, size, color);
            addQuad(v, cx,         cy + 0.04f, size, size, color);
            break;
        }
        case 4: {
            float size = 0.035f;
            addQuad(v, cx - 0.05f, cy - 0.05f, size, size, color);
            addQuad(v, cx + 0.05f, cy - 0.05f, size, size, color);
            addQuad(v, cx - 0.05f, cy + 0.05f, size, size, color);
            addQuad(v, cx + 0.05f, cy + 0.05f, size, size, color);
            break;
        }
        case 5: {
            float size = 0.03f;
            addQuad(v, cx - 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy,         size, size, color);
            addQuad(v, cx - 0.06f, cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy + 0.06f, size, size, color);
            break;
        }
        case 6: {
            float size = 0.025f;
            addQuad(v, cx - 0.05f, cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.05f, cy - 0.06f, size, size, color);
            addQuad(v, cx - 0.05f, cy,         size, size, color);
            addQuad(v, cx + 0.05f, cy,         size, size, color);
            addQuad(v, cx - 0.05f, cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.05f, cy + 0.06f, size, size, color);
            break;
        }
        case 7: {
            float size = 0.022f;
            addQuad(v, cx - 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy,         size, size, color);
            addQuad(v, cx - 0.06f, cy + 0.06f, size, size, color);
            addQuad(v, cx,         cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy + 0.06f, size, size, color);
            break;
        }
        case 8: {
            float size = 0.022f;
            addQuad(v, cx - 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx - 0.04f, cy,         size, size, color);
            addQuad(v, cx + 0.04f, cy,         size, size, color);
            addQuad(v, cx - 0.06f, cy + 0.06f, size, size, color);
            addQuad(v, cx,         cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy + 0.06f, size, size, color);
            break;
        }
        case 9: {
            float size = 0.022f;
            for (int r = -1; r <= 1; r++) {
                for (int c = -1; c <= 1; c++) {
                    addQuad(v, cx + c * 0.06f, cy + r * 0.06f, size, size, color);
                }
            }
            break;
        }
        default: {
            addQuad(v, cx, cy, 0.10f, 0.10f, color);
            return;
        }
    }
}

void GameView::render(int winW, int winH, float dragMX, float dragMY,
                      bool showPointer, const float pointerColor[3]) {
    float aspect = (float)winW / (float)winH;
    static float verts[GameModel::GRID * GameModel::GRID * 12 * 6 * 5];
    float* v = verts;

    for (int r = 0; r < GameModel::GRID; r++) {
        for (int c = 0; c < GameModel::GRID; c++) {
            if (r == _model.dragRow() && c == _model.dragCol()) continue;
            float cx = gridToWorldX(c);
            float cy = gridToWorldY(r);

            addQuad(v, cx, cy, _halfRender, _halfRender, _grey);

            const auto& n = _model.node(_model.rootChild(r, c));
            if (n.type == CellType::ITEM) {
                const float* col = _elemColors[n.data.item.id];
                renderCellItems(v, cx, cy, n.data.item.count, col);
            }
        }
    }

    if (_model.hasDrag()) {
        const float* col = _elemColors[_model.dragItemId()];
        if (_model.dragRow() >= 0 && _model.dragCol() >= 0) {
            float cx = gridToWorldX(_model.dragCol());
            float cy = gridToWorldY(_model.dragRow());
            addQuad(v, cx, cy, _halfRender, _halfRender, _grey);
        }
        renderCellItems(v, dragMX, dragMY, _model.dragAmount(), col);
    }

    if (showPointer) {
        float indicatorSize = _halfRender * 0.5f;
        addQuad(v, dragMX, dragMY, indicatorSize, indicatorSize, pointerColor);
    }

    int totalFloats = (int)(v - verts);
    if (totalFloats == 0) return;

    glUseProgram(_prog);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, totalFloats * sizeof(float), verts, GL_STREAM_DRAW);

    glUniform2f(_uPosLoc, _panX, _panY);
    glUniform1f(glGetUniformLocation(_prog, "uAspect"), aspect);
    glUniform1f(_uZoomLoc, _zoom);
    glVertexAttribPointer(_aPosLoc, 2, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float), 0);
    glEnableVertexAttribArray(_aPosLoc);
    glVertexAttribPointer(_aColorLoc, 3, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(_aColorLoc);

    glDrawArrays(GL_TRIANGLES, 0, totalFloats / 5);

    glDisableVertexAttribArray(_aColorLoc);
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

bool GameView::screenToGrid(int px, int py, int winW, int winH, int& row, int& col) const {
    float wx, wy;
    screenToWorld(px, py, winW, winH, wx, wy);
    col = (int)((wx - _gridMin) / _cellSize);
    row = (int)((wy - _gridMin) / _cellSize);
    if (col < 0 || col >= GameModel::GRID || row < 0 || row >= GameModel::GRID) return false;
    float cx = _gridMin + col * _cellSize + _cellSize * 0.5f;
    float cy = _gridMin + row * _cellSize + _cellSize * 0.5f;
    if (wx < cx - _halfRender || wx > cx + _halfRender ||
        wy < cy - _halfRender || wy > cy + _halfRender)
        return false;
    return true;
}

void GameView::screenToWorld(int px, int py, int winW, int winH, float& wx, float& wy) const {
    float aspect = (float)winW / (float)winH;
    wx = ((2.0f * px / (float)winW) - 1.0f - _panX) / _zoom;
    wy = ((1.0f - (2.0f * py / (float)winH)) - _panY) / aspect / _zoom;
}

float GameView::gridToWorldX(int col) const {
    return _gridMin + col * _cellSize + _cellSize * 0.5f;
}

float GameView::gridToWorldY(int row) const {
    return _gridMin + row * _cellSize + _cellSize * 0.5f;
}
