#pragma once
#include <blend2d/blend2d.h>
#include <cstdint>

// Build the shared 8x8 bitmap-font glyph atlas (once). Safe to call repeatedly.
void initFont();

// Draw a string through an existing (live, unscaled) context at pixel (x, y).
// Uses nearest-neighbour sampling and SRC_OVER onto the current target.
void drawText(BLContext &ctx, float x, float y, const char *text, float scale);

// Text width in pixels for a given scale (monospaced 8px font).
float textWidth(const char *text, float scale);