#pragma once

#include <blend2d.h>
#include <string>
#include <vector>

// Load an SVG document from raw bytes and rasterize it into `out` at its
// intrinsic size (svg2b2d's parseSVG). Returns true on success.
bool svgLoadFromBytes(const void* data, size_t size, BLImage& out);

// Rasterize an SVG (raw bytes) into `out` scaled to fit an image of
// `targetW`x`targetH` pixels, preserving aspect ratio and centered. Rendering
// the vector paths at the requested resolution keeps the result crisp even at
// high zoom — no bitmap upscaling. Returns true on success.
bool svgLoadFromBytesAtSize(const void* data, size_t size, int targetW, int targetH, BLImage& out);

// Load an SVG from a file path and rasterize into `out`. Returns true on success.
bool svgLoadFromFile(const std::string& path, BLImage& out);

// Convenience: draw `img` onto `ctx` scaled to fit the rect (preserving aspect,
// centered, using alpha blending). Useful for placing SVG assets on cells.
void svgDrawScaled(BLContext& ctx, const BLImage& img, const BLRect& rect);