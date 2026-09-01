#pragma once
#include <cstdint>
class SkCanvas;
#include "game/framebench.hpp"

// Build the shared 8x8 bitmap-font glyph atlas (once). Safe to call repeatedly.
void initFont();

// Draw a string through the given canvas at pixel (x, y) with an identity/1:1
// matrix. Uses the system vector font (white) via SkTextBlob. When `bench` is
// non-null, the vector-font path accumulates its stage timings there.
void drawText(SkCanvas* canvas, float x, float y, const char* text, float scale,
              Framebench* bench = nullptr);

// Text width in pixels for a given scale.
float textWidth(const char* text, float scale, Framebench* bench = nullptr);

// Text height (ascent + descent) in pixels for the current font at scale.
float textHeight(float scale, Framebench* bench = nullptr);