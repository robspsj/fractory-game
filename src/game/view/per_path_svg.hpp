#pragma once
#include <string>
#include <vector>
#include <memory>
#include "include/core/SkPath.h"
#include "include/core/SkPaint.h"
#if defined(__EMSCRIPTEN__)
#include "include/core/SkImage.h"
#endif
class SkCanvas;

struct PerPathSvg {
  struct PathInfo {
    SkPath path;
    SkPaint paint;
    SkRect bounds; // viewBox bounds for pivot
#if defined(__EMSCRIPTEN__)
    // Wasm has no GPU: re-scanline-AA of the curve set every frame dominates.
    // Each path is rasterized once per pow2 zoom bucket into this sprite and
    // then blitted (sway transforms still applied as canvas transforms).
    mutable sk_sp<SkImage> sprite;
    mutable float spriteScale = 0.0f; // px/viewBox-unit the sprite was built at
#endif
  };
  float viewBoxW = 64.0f;
  float viewBoxH = 64.0f;
  std::vector<PathInfo> paths;

  static std::unique_ptr<PerPathSvg> Load(const std::string &filePath);
  void render(SkCanvas *canvas, float px, float py, float rPx, float baseAngle, double tSec, int seed, uint8_t heat = 0, bool perPathSway = true) const;
};

// Global per-path sway angle for path i (different frequency per path)
float perPathSwayAngle(int pathIdx, int totalPaths, double tSec, int seed, uint8_t heat = 0);
