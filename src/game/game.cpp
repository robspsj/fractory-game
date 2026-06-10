#include "game.hpp"
#include "../shader.hpp"
#include <cstdlib>
#include <ctime>
#include <cstring>

#define GRID 5
#define ELEMS 5

static int grid[GRID][GRID];

static const float elemColors[ELEMS][3] = {
    {0.15f, 0.50f, 0.90f},
    {0.90f, 0.30f, 0.30f},
    {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f},
    {0.80f, 0.30f, 0.85f},
};

static const float gridMin = -0.78f;
static const float cellSize = 1.56f / GRID;
static const float gap = 0.03f;
static const float halfRender = (cellSize - gap) * 0.5f;

static int hoverRow = -1, hoverCol = -1;
static int dragRow = -1, dragCol = -1;
static float dragMX = 0.0f, dragMY = 0.0f;

static GLuint prog, vbo;
static GLint aPosLoc, aColorLoc, uPosLoc;

void gameInit() {
    std::srand((unsigned)std::time(nullptr));
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            grid[r][c] = std::rand() % ELEMS;

    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vertSrc));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fragSrc));
    glLinkProgram(prog);
    glUseProgram(prog);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    aPosLoc = glGetAttribLocation(prog, "aPos");
    aColorLoc = glGetAttribLocation(prog, "aColor");
    uPosLoc = glGetUniformLocation(prog, "uPos");
}

static int cellAt(int px, int py, int winW, int winH) {
    float nx = (2.0f * px / (float)winW) - 1.0f;
    float ny = 1.0f - (2.0f * py / (float)winH);
    int col = (int)((nx - gridMin) / cellSize);
    int row = (int)((ny - gridMin) / cellSize);
    if (col < 0 || col >= GRID || row < 0 || row >= GRID) return -1;
    float cx = gridMin + col * cellSize + cellSize * 0.5f;
    float cy = gridMin + row * cellSize + cellSize * 0.5f;
    if (nx < cx - halfRender || nx > cx + halfRender ||
        ny < cy - halfRender || ny > cy + halfRender)
        return -1;
    return row * GRID + col;
}

void gameUpdate(int mousePx, int mousePy, int winW, int winH) {
    dragMX = (2.0f * mousePx / (float)winW) - 1.0f;
    dragMY = 1.0f - (2.0f * mousePy / (float)winH);
    int idx = cellAt(mousePx, mousePy, winW, winH);
    if (idx >= 0) {
        hoverRow = idx / GRID;
        hoverCol = idx % GRID;
    } else {
        hoverRow = hoverCol = -1;
    }
}

void gameMouseDown(int mousePx, int mousePy, int winW, int winH) {
    int idx = cellAt(mousePx, mousePy, winW, winH);
    if (idx >= 0) {
        dragRow = idx / GRID;
        dragCol = idx % GRID;
    }
}

void gameMouseUp(int mousePx, int mousePy, int winW, int winH) {
    if (dragRow >= 0 && dragCol >= 0) {
        int idx = cellAt(mousePx, mousePy, winW, winH);
        if (idx >= 0) {
            int tr = idx / GRID, tc = idx % GRID;
            int tmp = grid[dragRow][dragCol];
            grid[dragRow][dragCol] = grid[tr][tc];
            grid[tr][tc] = tmp;
        }
    }
    dragRow = dragCol = -1;
}

static void addQuad(float *&v, float cx, float cy, const float color[3]) {
    float h = halfRender;
    for (int i = 0; i < 6; i++) {
        float dx = (i == 1 || i == 3 || i == 4) ? h : -h;
        float dy = (i == 0 || i == 1 || i == 4) ? -h : h;
        v[0] = cx + dx; v[1] = cy + dy;
        v[2] = color[0]; v[3] = color[1]; v[4] = color[2];
        v += 5;
    }
}

void gameRender() {
    float verts[26 * 6 * 5];
    float *v = verts;

    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (r == dragRow && c == dragCol) continue;
            float cx = gridMin + c * cellSize + cellSize * 0.5f;
            float cy = gridMin + r * cellSize + cellSize * 0.5f;
            const float *col = elemColors[grid[r][c]];
            float hc[3] = {col[0], col[1], col[2]};
            if (r == hoverRow && c == hoverCol && dragRow < 0) {
                float bright = 1.4f;
                hc[0] = hc[0] * bright > 1.0f ? 1.0f : hc[0] * bright;
                hc[1] = hc[1] * bright > 1.0f ? 1.0f : hc[1] * bright;
                hc[2] = hc[2] * bright > 1.0f ? 1.0f : hc[2] * bright;
            }
            addQuad(v, cx, cy, hc);
        }
    }

    if (dragRow >= 0 && dragCol >= 0) {
        const float *col = elemColors[grid[dragRow][dragCol]];
        float dim[3] = {col[0] * 0.5f, col[1] * 0.5f, col[2] * 0.5f};
        float cx = gridMin + dragCol * cellSize + cellSize * 0.5f;
        float cy = gridMin + dragRow * cellSize + cellSize * 0.5f;
        addQuad(v, cx, cy, dim);
        addQuad(v, dragMX, dragMY, col);
    }

    int totalFloats = (int)(v - verts);
    if (totalFloats == 0) return;

    glUseProgram(prog);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, totalFloats * sizeof(float), verts, GL_STREAM_DRAW);

    glUniform2f(uPosLoc, 0.0f, 0.0f);
    glVertexAttribPointer(aPosLoc, 2, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float), 0);
    glEnableVertexAttribArray(aPosLoc);
    glVertexAttribPointer(aColorLoc, 3, GL_FLOAT, GL_FALSE, 5 * (int)sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(aColorLoc);

    glDrawArrays(GL_TRIANGLES, 0, totalFloats / 5);

    glDisableVertexAttribArray(aColorLoc);
}

GLuint gameProgram() {
    return prog;
}

void gameSetState(int newGrid[GRID][GRID]) {
    std::memcpy(grid, newGrid, sizeof(grid));
}

void gameGetState(int outGrid[GRID][GRID]) {
    std::memcpy(outGrid, grid, sizeof(grid));
}
