#pragma once
#include <string>
#include <vector>
#include <memory>
#include "include/core/SkPath.h"
#include "include/core/SkPaint.h"
class SkCanvas;

struct PerPathSvg {
  struct PathInfo {
    SkPath path;
    SkPaint paint;
    SkRect bounds; // viewBox bounds for pivot
  };
  float viewBoxW = 64.0f;
  float viewBoxH = 64.0f;
  std::vector<PathInfo> paths;

  static std::unique_ptr<PerPathSvg> Load(const std::string &filePath);
  void render(SkCanvas *canvas, float px, float py, float rPx, float baseAngle, double tSec, int seed) const;
};

// Global per-path sway angle for path i (different frequency per path)
float perPathSwayAngle(int pathIdx, int totalPaths, double tSec, int seed);
