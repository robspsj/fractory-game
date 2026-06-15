#include "game.hpp"
#include "item_renderer.hpp"
#include "../shader.hpp"
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>

#define GRID 5
#define ELEMS 5

static std::vector<Cell> cells;

static const float elemColors[ELEMS][3] = {
    {0.15f, 0.50f, 0.90f},
    {0.90f, 0.30f, 0.30f},
    {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f},
    {0.80f, 0.30f, 0.85f},
};

static const float gridMin = -0.75f;
static const float cellSize = 0.30f;
static const float gap = 0.02f;
static const float halfRender = (cellSize - gap) * 0.5f;

static int hoverRow = -1, hoverCol = -1;
static int dragRow = -1, dragCol = -1;
static float dragMX = 0.0f, dragMY = 0.0f;

static GLuint prog, vbo;
static GLint aPosLoc, aColorLoc, uPosLoc;

static void stateInit(unsigned int seed) {
    if (seed != 0) std::srand(seed);
    else std::srand((unsigned)std::time(nullptr));

    cells.resize(GRID * GRID);

    int subgridCount = 0;
    for (int i = 0; i < GRID * GRID; i++) {
        int r = i / GRID;
        int c = i % GRID;
        int randVal = std::rand() % 100;
        if (randVal < 50) {
            cells[i].type = CellType::EMPTY;
        } else if (randVal < 90 || subgridCount >= 20) {
            cells[i].type = CellType::ITEM;
            cells[i].data.item.id = std::rand() % ELEMS;
            cells[i].data.item.count = std::rand() % 9 + 1;
        } else {
            cells[i].type = CellType::SUBGRID;
            cells[i].data.subgrid.data = nullptr; // Initialize as null for now
            cells[i].data.subgrid.size = 3;
            subgridCount++;
        }
    }
}

static void graphicsInit() {
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

void gameInit(unsigned int seed) {
    stateInit(seed);
    graphicsInit();
}

static int cellAt(int px, int py, int winW, int winH) {
    float aspect = (float)winW / (float)winH;
    float nx = (2.0f * px / (float)winW) - 1.0f;
    float ny = (1.0f - (2.0f * py / (float)winH)) / aspect;
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
    float aspect = (float)winW / (float)winH;
    dragMX = (2.0f * mousePx / (float)winW) - 1.0f;
    dragMY = (1.0f - (2.0f * mousePy / (float)winH)) / aspect;
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
            Cell tmp = cells[dragRow * GRID + dragCol];
            cells[dragRow * GRID + dragCol] = cells[idx];
            cells[idx] = tmp;
        }
    }
    dragRow = dragCol = -1;
}

static const float grey[] = {0.5f, 0.5f, 0.5f};
static const float itemSize = 0.20f;
static const float halfItem = itemSize * 0.5f;

static void addQuad(float *&v, float cx, float cy, float w, float h, const float color[3]) {
    for (int i = 0; i < 6; i++) {
        float dx = (i == 1 || i == 3 || i == 4) ? w : -w;
        float dy = (i == 0 || i == 1 || i == 4) ? -h : h;
        v[0] = cx + dx; v[1] = cy + dy;
        v[2] = color[0]; v[3] = color[1]; v[4] = color[2];
        v += 5;
    }
}

void gameRender(int winW, int winH) {
    float aspect = (float)winW / (float)winH;
    static float verts[GRID * GRID * 12 * 6 * 5];
    float *v = verts;

    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            int idx = r * GRID + c;
            if (r == dragRow && c == dragCol) continue;
            float cx = gridMin + c * cellSize + cellSize * 0.5f;
            float cy = gridMin + r * cellSize + cellSize * 0.5f;

            // Draw empty square (background)
            addQuad(v, cx, cy, halfRender, halfRender, grey);

            // Draw colored item inside
            if (cells[idx].type == CellType::ITEM) {
                const float *col = elemColors[cells[idx].data.item.id];
                renderCellItems(v, cx, cy, cells[idx].data.item.count, col);
            }
        }
    }

    if (dragRow >= 0 && dragCol >= 0) {
        int dragIdx = dragRow * GRID + dragCol;
        if (cells[dragIdx].type == CellType::ITEM) {
            const float *col = elemColors[cells[dragIdx].data.item.id];
            float cx = gridMin + dragCol * cellSize + cellSize * 0.5f;
            float cy = gridMin + dragRow * cellSize + cellSize * 0.5f;
            
            // Drag source cell shows just the background
            addQuad(v, cx, cy, halfRender, halfRender, grey);

            // Dragged item shows just the colored square(s)
            renderCellItems(v, dragMX, dragMY, cells[dragIdx].data.item.count, col);
        }
    }

    int totalFloats = (int)(v - verts);
    if (totalFloats == 0) return;

    glUseProgram(prog);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, totalFloats * sizeof(float), verts, GL_STREAM_DRAW);

    glUniform2f(uPosLoc, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(prog, "uAspect"), aspect);
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

void gameSetState(int* newGrid) {
    for (int i = 0; i < GRID * GRID; i++) {
        cells[i].type = CellType::ITEM;
        cells[i].data.item.id = newGrid[i];
        cells[i].data.item.count = 1;
    }
}

void gameGetState(int* outGrid) {
    for (int i = 0; i < GRID * GRID; i++) {
        outGrid[i] = (cells[i].type == CellType::ITEM) ? cells[i].data.item.id : 0;
    }
}
