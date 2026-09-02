#include "per_path_svg.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include "include/core/SkCanvas.h"
#include "include/utils/SkParsePath.h"
#include "include/core/SkColor.h"

static SkColor parseHexFill(const std::string &s, SkColor fallback = SK_ColorBLACK) {
  if (s.empty() || s == "none") return SK_ColorTRANSPARENT;
  std::string hex = s;
  if (hex[0] == '#') hex = hex.substr(1);
  if (hex.size() == 6) {
    unsigned r = std::stoul(hex.substr(0,2), nullptr, 16);
    unsigned g = std::stoul(hex.substr(2,2), nullptr, 16);
    unsigned b = std::stoul(hex.substr(4,2), nullptr, 16);
    return SkColorSetRGB(r,g,b);
  }
  if (hex.size() == 3) {
    unsigned r = std::stoul(hex.substr(0,1), nullptr, 16) * 17;
    unsigned g = std::stoul(hex.substr(1,1), nullptr, 16) * 17;
    unsigned b = std::stoul(hex.substr(2,1), nullptr, 16) * 17;
    return SkColorSetRGB(r,g,b);
  }
  return fallback;
}

static std::string resolveAssetPath(const std::string &path) {
  namespace fs = std::filesystem;
  fs::path rel(path);
  std::error_code ec;
  if (fs::exists(rel, ec)) return rel.string();
  fs::path dir = fs::current_path(ec);
  while (true) {
    fs::path cand = dir / rel;
    if (fs::exists(cand, ec)) return cand.string();
    if (!dir.has_parent_path()) break;
    dir = dir.parent_path();
  }
  return path;
}

std::unique_ptr<PerPathSvg> PerPathSvg::Load(const std::string &filePath) {
  std::string real = resolveAssetPath(filePath);
  std::ifstream in(real);
  if (!in) return nullptr;
  std::stringstream ss; ss << in.rdbuf();
  std::string content = ss.str();
  if (content.empty()) return nullptr;

  auto result = std::make_unique<PerPathSvg>();

  // Parse viewBox="0 0 64 64" -> viewBoxW/H
  size_t vbPos = content.find("viewBox=");
  if (vbPos != std::string::npos) {
    size_t q1 = content.find('"', vbPos);
    size_t q2 = content.find('"', q1+1);
    if (q1 != std::string::npos && q2 != std::string::npos) {
      std::string vb = content.substr(q1+1, q2-q1-1);
      std::stringstream vbs(vb);
      float x,y,w,h;
      if (vbs >> x >> y >> w >> h) { result->viewBoxW = w; result->viewBoxH = h; }
    }
  }

  // Extract each <path ...> tag
  size_t pos = 0;
  while (true) {
    size_t pStart = content.find("<path", pos);
    if (pStart == std::string::npos) break;
    size_t pEnd = content.find('>', pStart);
    if (pEnd == std::string::npos) break;
    std::string tag = content.substr(pStart, pEnd - pStart + 1);

    // d="..."
    std::string d;
    size_t dPos = tag.find("d=\"");
    if (dPos == std::string::npos) dPos = tag.find("d='");
    if (dPos != std::string::npos) {
      char q = tag[dPos+2];
      if (q == '"' || q == '\'') {
        size_t dStart = dPos+3;
        size_t dEnd = tag.find(q, dStart);
        if (dEnd != std::string::npos) d = tag.substr(dStart, dEnd - dStart);
      } else {
        // fallback for d=" without quote char check
        size_t dStart = tag.find('"', dPos);
        size_t dEnd = tag.find('"', dStart+1);
        if (dStart != std::string::npos && dEnd != std::string::npos) d = tag.substr(dStart+1, dEnd-dStart-1);
      }
    }
    if (d.empty()) { pos = pEnd+1; continue; }

    auto optPath = SkParsePath::FromSVGString(d.c_str());
    if (!optPath) { pos = pEnd+1; continue; }

    // fill="..."
    std::string fillStr;
    size_t fPos = tag.find("fill=\"");
    if (fPos == std::string::npos) fPos = tag.find("fill='");
    if (fPos != std::string::npos) {
      char q = tag[fPos+5];
      size_t fStart = fPos+6;
      size_t fEnd = tag.find(q, fStart);
      if (fEnd != std::string::npos) fillStr = tag.substr(fStart, fEnd - fStart);
    }
    SkColor fill = fillStr.empty() ? SK_ColorBLACK : parseHexFill(fillStr, SK_ColorBLACK);
    if (fill == SK_ColorTRANSPARENT) { pos = pEnd+1; continue; }

    PathInfo pi;
    pi.path = *optPath;
    pi.paint.setAntiAlias(true);
    pi.paint.setColor(fill);
    pi.paint.setStyle(SkPaint::kFill_Style);
    pi.bounds = pi.path.getBounds();
    result->paths.push_back(std::move(pi));

    pos = pEnd+1;
  }

  if (result->paths.empty()) return nullptr;
  return result;
}

float perPathSwayAngle(int pathIdx, int totalPaths, double tSec, int seed) {
  // Each path gets a distinct frequency and phase. Base sway derived from
  // global time plus per-path offset. Frequencies spread 0.3..0.9 Hz.
  double baseFreq = 0.35 + (double)(pathIdx % 7) * 0.08; // 0.35,0.43,0.51...
  // Add seed-based phase to desync instances of same icon at different positions
  double phase = (double)((seed * 9781 + pathIdx * 6151) % 6283) / 1000.0; // 0..2pi
  double amp = 4.0 + (double)(pathIdx % 5) * 1.2; // 4..8.8 deg
  // Also add a small per-path offset based on totalPaths to keep motion coherent
  return (float)(amp * std::sin(2.0 * 3.141592653589793 * baseFreq * tSec + phase));
}

void PerPathSvg::render(SkCanvas *canvas, float px, float py, float rPx, float baseAngle, double tSec, int seed) const {
  if (!canvas || paths.empty()) return;
  // Map viewBox (e.g., 64) to pixel size side = 2*rPx
  float side = 2.0f * (float)rPx;
  float vb = std::max(viewBoxW, viewBoxH);
  if (vb < 1.0f) vb = 64.0f;
  float s = side / vb;

  canvas->save();
  canvas->resetMatrix();
  canvas->translate((SkScalar)px, (SkScalar)py);
  canvas->rotate((SkScalar)baseAngle);
  // Scale viewBox to pixel, then center
  canvas->scale((SkScalar)s, (SkScalar)s);
  canvas->translate((SkScalar)(-viewBoxW*0.5f), (SkScalar)(-viewBoxH*0.5f));

  int n = (int)paths.size();
  for (int i = 0; i < n; i++) {
    const auto &pi = paths[i];
    // Per-path sway: each path rotates around its own center with distinct frequency
    float ang = perPathSwayAngle(i, n, tSec, seed);
    // Also add a small per-path translation sway (horizontal) for more visible effect
    float transAmp = 1.5f; // viewBox units
    double transFreq = 0.5 + (double)(i % 4) * 0.15;
    double transPhase = (double)((seed * 31 + i * 137) % 6283) / 1000.0;
    float tx = (float)(transAmp * std::sin(2.0 * 3.141592653589793 * transFreq * tSec + transPhase));
    // Pivot around path center
    SkPoint c = {pi.bounds.centerX(), pi.bounds.centerY()};
    canvas->save();
    canvas->translate(c.x(), c.y());
    canvas->rotate((SkScalar)ang);
    canvas->translate((SkScalar)tx, 0);
    canvas->translate(-c.x(), -c.y());
    canvas->drawPath(pi.path, pi.paint);
    canvas->restore();
  }
  canvas->restore();
}
