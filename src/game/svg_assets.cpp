#include "svg_assets.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

// svg2b2d's public entry points (linked from third_party/svg2b2d/svg.cpp).
extern "C" bool parseSVG(const void* bytes, size_t sz, BLImage& outImage);
extern "C" bool parseSVGscaled(const void* bytes, size_t sz, int targetW, int targetH, BLImage& outImage);

bool svgLoadFromBytes(const void* data, size_t size, BLImage& out) {
  if (!data || size == 0)
    return false;
  return parseSVG(data, size, out);
}

bool svgLoadFromBytesAtSize(const void* data, size_t size, int targetW, int targetH, BLImage& out) {
  if (!data || size == 0)
    return false;
  return parseSVGscaled(data, size, targetW, targetH, out);
}

bool svgLoadFromFile(const std::string& path, BLImage& out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) {
    fclose(f);
    return false;
  }
  std::vector<uint8_t> buf((size_t)n);
  bool ok = fread(buf.data(), 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  if (!ok)
    return false;
  return svgLoadFromBytes(buf.data(), buf.size(), out);
}

void svgDrawScaled(BLContext& ctx, const BLImage& img, const BLRect& rect) {
  if (img.empty())
    return;
  double iw = (double)img.width();
  double ih = (double)img.height();
  if (iw <= 0 || ih <= 0 || rect.w <= 0 || rect.h <= 0)
    return;
  // Fit inside rect, preserving aspect ratio, centered.
  double scale = std::min(rect.w / iw, rect.h / ih);
  double dw = iw * scale, dh = ih * scale;
  double dx = rect.x + (rect.w - dw) * 0.5;
  double dy = rect.y + (rect.h - dh) * 0.5;
  ctx.blitImage(BLRect(dx, dy, dw, dh), img, BLRectI(0, 0, img.width(), img.height()));
}