#pragma once
#include <cstdint>
class SkCanvas;

// Build the shared 8x8 bitmap-font glyph atlas (once). Safe to call repeatedly.
void initFont();

// Draw a string through the given canvas at pixel (x, y) with an identity/1:1
// matrix. Uses nearest-style 8x8 rect glyphs (white) with SRC_OVER blending.
void drawText(SkCanvas* canvas, float x, float y, const char* text, float scale);

// Text width in pixels for a given scale (monospaced 8px font).
float textWidth(const char* text, float scale);