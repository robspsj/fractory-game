#include "item_renderer.hpp"

static void addQuad(float *&v, float cx, float cy, float w, float h, const float color[3]) {
    for (int i = 0; i < 6; i++) {
        float dx = (i == 1 || i == 3 || i == 4) ? w : -w;
        float dy = (i == 0 || i == 1 || i == 4) ? -h : h;
        v[0] = cx + dx; v[1] = cy + dy;
        v[2] = color[0]; v[3] = color[1]; v[4] = color[2];
        v += 5;
    }
}

void renderCellItems(float *&v, float cx, float cy, int count, const float color[3]) {
    if (count <= 0) return;

    if (count >= 10) {
        // Draw a single large item in the center
        addQuad(v, cx, cy, 0.10f, 0.10f, color);
        return;
    }

    switch (count) {
        case 1: {
            addQuad(v, cx, cy, 0.08f, 0.08f, color);
            break;
        }
        case 2: {
            // Two side-by-side
            float size = 0.04f;
            addQuad(v, cx - 0.05f, cy, size, size, color);
            addQuad(v, cx + 0.05f, cy, size, size, color);
            break;
        }
        case 3: {
            // Three in triangle
            float size = 0.035f;
            addQuad(v, cx - 0.05f, cy - 0.04f, size, size, color);
            addQuad(v, cx + 0.05f, cy - 0.04f, size, size, color);
            addQuad(v, cx,         cy + 0.04f, size, size, color);
            break;
        }
        case 4: {
            // 2x2 grid
            float size = 0.035f;
            addQuad(v, cx - 0.05f, cy - 0.05f, size, size, color);
            addQuad(v, cx + 0.05f, cy - 0.05f, size, size, color);
            addQuad(v, cx - 0.05f, cy + 0.05f, size, size, color);
            addQuad(v, cx + 0.05f, cy + 0.05f, size, size, color);
            break;
        }
        case 5: {
            // 5 on a die
            float size = 0.03f;
            addQuad(v, cx - 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy,         size, size, color);
            addQuad(v, cx - 0.06f, cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy + 0.06f, size, size, color);
            break;
        }
        case 6: {
            // 2x3 grid
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
            // 3-1-3 layout
            float size = 0.022f;
            // bottom row (3)
            addQuad(v, cx - 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy - 0.06f, size, size, color);
            // middle row (1)
            addQuad(v, cx,         cy,         size, size, color);
            // top row (3)
            addQuad(v, cx - 0.06f, cy + 0.06f, size, size, color);
            addQuad(v, cx,         cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy + 0.06f, size, size, color);
            break;
        }
        case 8: {
            // 3-2-3 layout
            float size = 0.022f;
            // bottom row (3)
            addQuad(v, cx - 0.06f, cy - 0.06f, size, size, color);
            addQuad(v, cx,         cy - 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy - 0.06f, size, size, color);
            // middle row (2)
            addQuad(v, cx - 0.04f, cy,         size, size, color);
            addQuad(v, cx + 0.04f, cy,         size, size, color);
            // top row (3)
            addQuad(v, cx - 0.06f, cy + 0.06f, size, size, color);
            addQuad(v, cx,         cy + 0.06f, size, size, color);
            addQuad(v, cx + 0.06f, cy + 0.06f, size, size, color);
            break;
        }
        case 9: {
            // 3x3 grid
            float size = 0.022f;
            for (int r = -1; r <= 1; r++) {
                for (int c = -1; c <= 1; c++) {
                    addQuad(v, cx + c * 0.06f, cy + r * 0.06f, size, size, color);
                }
            }
            break;
        }
    }
}
