#pragma once

#include <blend2d.h>
#include <string>

// Load an SVG document from raw bytes and rasterize it into `out` at its
// intrinsic size (svg2b2d's parseSVG). Returns true on success.
bool svgLoadFromBytes(const void* data, size_t size, BLImage& out);

// Load an SVG from a file path and rasterize into `out`. Returns true on success.
bool svgLoadFromFile(const std::string& path, BLImage& out);

// Convenience: draw `img` onto `ctx` scaled to fit the rect (preserving aspect,
// centered, using alpha blending). Useful for placing SVG assets on cells.
void svgDrawScaled(BLContext& ctx, const BLImage& img, const BLRect& rect);