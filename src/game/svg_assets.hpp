#pragma once

#include <blend2d.h>
#include <string>
#include <vector>

// Rasterize an SVG (raw bytes) into `out` scaled to fit an image of
// `targetW`x`targetH` pixels, preserving aspect ratio and centered. Rendering
// the vector paths at the requested resolution keeps the result crisp even at
// high zoom — no bitmap upscaling. Returns true on success.
bool svgLoadFromBytesAtSize(const void* data, size_t size, int targetW, int targetH, BLImage& out);