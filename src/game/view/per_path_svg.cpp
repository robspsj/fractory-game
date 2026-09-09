#include "per_path_svg.hpp"
#include "../heat_constants.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include "include/core/SkCanvas.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/utils/SkParsePath.h"
#include "include/core/SkColor.h"
#if defined(__EMSCRIPTEN__)
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#endif

#if defined(__EMSCRIPTEN__)
// CPU-raster only: blit a pre-rasterized sprite instead of re-running the
// anti-aliased curve scanline pass every frame. Resolution is bucketed to
// powers of two so zooming rebuilds at most once per octave.
static void drawPathOrSprite(SkCanvas *canvas, const PerPathSvg::PathInfo &pi, float s) {
  if (!(s > 0.001f)) return;
  SkRect bb = pi.bounds;
  if (pi.paint.getStyle() != SkPaint::kFill_Style) {
    float sw = std::max(pi.paint.getStrokeWidth(), 1.0f);
    bb.outset(sw, sw);
  }
  if (bb.isEmpty()) return;
  const float maxSide = std::max(bb.width(), bb.height());
  if (maxSide * s > 4096.0f) { canvas->drawPath(pi.path, pi.paint); return; }
  float q = std::exp2f(std::floorf(std::log2f(s)));
  if (!pi.sprite || pi.spriteScale < q) {
    q = std::min(q, 2048.0f / maxSide);
    int iw = std::max(1, (int)std::ceil(bb.width()  * q));
    int ih = std::max(1, (int)std::ceil(bb.height() * q));
    auto surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(iw, ih));
    if (!surf) { canvas->drawPath(pi.path, pi.paint); return; }
    SkCanvas *pc = surf->getCanvas();
    pc->scale(q, q);
    pc->translate(-bb.left(), -bb.top());
    pc->drawPath(pi.path, pi.paint);
    pi.sprite = surf->makeImageSnapshot();
    pi.spriteScale = q;
  }
  canvas->drawImageRect(pi.sprite.get(),
      SkRect::MakeXYWH(bb.left(), bb.top(), bb.width(), bb.height()),
      SkFilterMode::kLinear);
}
#else
static inline void drawPathOrSprite(SkCanvas *canvas, const PerPathSvg::PathInfo &pi, float) {
  canvas->drawPath(pi.path, pi.paint);
}
#endif

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

static std::string getAttr(const std::string &tag, const std::string &name) {
  std::string key = name + "=";
  size_t pos = tag.find(key);
  if (pos == std::string::npos) return "";
  size_t qPos = pos + key.size();
  if (qPos >= tag.size()) return "";
  char q = tag[qPos];
  if (q == '"' || q == '\'') {
    size_t start = qPos + 1;
    size_t end = tag.find(q, start);
    if (end != std::string::npos) return tag.substr(start, end - start);
  } else {
    // unquoted
    size_t start = qPos;
    size_t end = tag.find_first_of(" \t>", start);
    if (end != std::string::npos) return tag.substr(start, end - start);
    return tag.substr(start);
  }
  return "";
}

static float parseFloat(const std::string &s, float def = 0.0f) {
  if (s.empty()) return def;
  try { return std::stof(s); } catch (...) { return def; }
}

static std::vector<std::pair<float,float>> parsePoints(const std::string &pts) {
  std::vector<std::pair<float,float>> out;
  std::string tmp = pts;
  for (char &c : tmp) if (c == ',') c = ' ';
  std::stringstream ss(tmp);
  float x,y;
  while (ss >> x >> y) out.emplace_back(x,y);
  return out;
}

std::unique_ptr<PerPathSvg> PerPathSvg::Load(const std::string &filePath) {
  std::string real = resolveAssetPath(filePath);
  std::ifstream in(real);
  if (!in) return nullptr;
  std::stringstream ss; ss << in.rdbuf();
  std::string content = ss.str();
  if (content.empty()) return nullptr;

  auto result = std::make_unique<PerPathSvg>();

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

  // Scan in document order for any shape tag: path, polygon, polyline, rect, circle, ellipse, line
  size_t pos = 0;
  while (true) {
    size_t lt = content.find('<', pos);
    if (lt == std::string::npos) break;
    size_t gt = content.find('>', lt);
    if (gt == std::string::npos) break;
    std::string tag = content.substr(lt, gt - lt + 1);
    // quick check for shape type (lowercase)
    std::string lowerTag = tag;
    for (auto &c : lowerTag) c = std::tolower(c);

    SkPath skPath;
    bool hasPath = false;
    std::string fillStr = getAttr(tag, "fill");

    // Also handle style="fill:#..." ? For now prefer fill attr, fallback to style
    if (fillStr.empty()) {
      std::string style = getAttr(tag, "style");
      if (!style.empty()) {
        size_t fpos = style.find("fill:");
        if (fpos != std::string::npos) {
          size_t start = fpos + 5;
          size_t end = style.find(';', start);
          fillStr = style.substr(start, end == std::string::npos ? std::string::npos : end - start);
          // trim
          size_t a = fillStr.find_first_not_of(" \t");
          size_t b = fillStr.find_last_not_of(" \t");
          if (a != std::string::npos) fillStr = fillStr.substr(a, b - a + 1);
        }
      }
    }

    if (lowerTag.rfind("<path", 0) == 0) {
      std::string d = getAttr(tag, "d");
      if (!d.empty()) {
        if (auto opt = SkParsePath::FromSVGString(d.c_str())) { skPath = *opt; hasPath = true; }
      }
    } else if (lowerTag.rfind("<polygon", 0) == 0) {
      std::string pts = getAttr(tag, "points");
      auto points = parsePoints(pts);
      if (points.size() >= 2) {
        SkPathBuilder b;
        b.moveTo(points[0].first, points[0].second);
        for (size_t i = 1; i < points.size(); i++) b.lineTo(points[i].first, points[i].second);
        b.close();
        skPath = b.detach();
        hasPath = true;
      }
    } else if (lowerTag.rfind("<polyline", 0) == 0) {
      std::string pts = getAttr(tag, "points");
      auto points = parsePoints(pts);
      if (points.size() >= 2) {
        SkPathBuilder b;
        b.moveTo(points[0].first, points[0].second);
        for (size_t i = 1; i < points.size(); i++) b.lineTo(points[i].first, points[i].second);
        skPath = b.detach();
        hasPath = true;
      }
    } else if (lowerTag.rfind("<rect", 0) == 0) {
      float x = parseFloat(getAttr(tag, "x"), 0);
      float y = parseFloat(getAttr(tag, "y"), 0);
      float w = parseFloat(getAttr(tag, "width"), 0);
      float h = parseFloat(getAttr(tag, "height"), 0);
      float rx = parseFloat(getAttr(tag, "rx"), 0);
      float ry = parseFloat(getAttr(tag, "ry"), rx);
      if (w > 0 && h > 0) {
        SkPathBuilder b;
        if (rx > 0 || ry > 0) {
          SkRRect rr; rr.setRectXY(SkRect::MakeXYWH(x, y, w, h), rx, ry);
          b.addRRect(rr);
        } else {
          b.addRect(SkRect::MakeXYWH(x, y, w, h));
        }
        skPath = b.detach();
        hasPath = true;
      }
    } else if (lowerTag.rfind("<circle", 0) == 0) {
      float cx = parseFloat(getAttr(tag, "cx"), 0);
      float cy = parseFloat(getAttr(tag, "cy"), 0);
      float r = parseFloat(getAttr(tag, "r"), 0);
      if (r > 0) { SkPathBuilder b; b.addCircle(cx, cy, r); skPath = b.detach(); hasPath = true; }
    } else if (lowerTag.rfind("<ellipse", 0) == 0) {
      float cx = parseFloat(getAttr(tag, "cx"), 0);
      float cy = parseFloat(getAttr(tag, "cy"), 0);
      float rx = parseFloat(getAttr(tag, "rx"), 0);
      float ry = parseFloat(getAttr(tag, "ry"), 0);
      if (rx > 0 && ry > 0) { SkPathBuilder b; b.addOval(SkRect::MakeXYWH(cx - rx, cy - ry, rx*2, ry*2)); skPath = b.detach(); hasPath = true; }
    } else if (lowerTag.rfind("<line", 0) == 0) {
      float x1 = parseFloat(getAttr(tag, "x1"), 0);
      float y1 = parseFloat(getAttr(tag, "y1"), 0);
      float x2 = parseFloat(getAttr(tag, "x2"), 0);
      float y2 = parseFloat(getAttr(tag, "y2"), 0);
      SkPathBuilder b; b.moveTo(x1, y1); b.lineTo(x2, y2);
      skPath = b.detach();
      hasPath = true;
      // lines are stroked, not filled — use stroke color if fill is none
      if (fillStr.empty() || fillStr == "none") {
        fillStr = getAttr(tag, "stroke");
      }
    }

    if (hasPath) {
      SkColor fill = fillStr.empty() ? SK_ColorBLACK : parseHexFill(fillStr, SK_ColorBLACK);
      // For lines, if still transparent, try stroke
      if (fill == SK_ColorTRANSPARENT) {
        std::string stroke = getAttr(tag, "stroke");
        if (!stroke.empty() && stroke != "none") fill = parseHexFill(stroke, SK_ColorBLACK);
      }
      if (fill == SK_ColorTRANSPARENT) { pos = gt + 1; continue; }
      PathInfo pi;
      pi.path = skPath;
      pi.paint.setAntiAlias(true);
      pi.paint.setColor(fill);
      // lines should be stroked
      if (lowerTag.rfind("<line", 0) == 0 || lowerTag.rfind("<polyline", 0) == 0) {
        pi.paint.setStyle(SkPaint::kStroke_Style);
        std::string sw = getAttr(tag, "stroke-width");
        float w = sw.empty() ? 1.0f : parseFloat(sw, 1.0f);
        pi.paint.setStrokeWidth(w);
      } else {
        pi.paint.setStyle(SkPaint::kFill_Style);
      }
      pi.bounds = pi.path.getBounds();
      result->paths.push_back(std::move(pi));
    }

    pos = gt + 1;
  }

  if (result->paths.empty()) return nullptr;
  return result;
}

float perPathSwayAngle(int pathIdx, int totalPaths, double tSec, int seed, uint8_t heat) {
  double heatF = (double)heat / 255.0;
  double baseFreq = 0.35 + (double)(pathIdx % 7) * 0.08;
  baseFreq *= (1.0 + heatF * Heat::Visual::SWAY_FREQ_SCALE);
  double phase = (double)((seed * 9781 + pathIdx * 6151) % 6283) / 1000.0;
  double amp = 4.0 + (double)(pathIdx % 5) * 1.2;
  amp *= (1.0 + heatF * Heat::Visual::SWAY_AMP_SCALE);
  return (float)(amp * std::sin(2.0 * 3.141592653589793 * baseFreq * tSec + phase));
}

void PerPathSvg::render(SkCanvas *canvas, float px, float py, float rPx, float baseAngle, double tSec, int seed, uint8_t heat, bool perPathSway) const {
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
  double heatF = (double)heat / 255.0;
  for (int i = 0; i < n; i++) {
    const auto &pi = paths[i];
    if (!perPathSway) { drawPathOrSprite(canvas, pi, s); continue; }
    // Per-path sway: each path rotates around its own center with distinct frequency
    float ang = perPathSwayAngle(i, n, tSec, seed, heat);
    float transAmp = 1.5f * (float)(1.0 + heatF * Heat::Visual::TRANS_AMP_SCALE);
    double transFreq = (0.5 + (double)(i % 4) * 0.15) * (1.0 + heatF * Heat::Visual::TRANS_FREQ_SCALE);
    double transPhase = (double)((seed * 31 + i * 137) % 6283) / 1000.0;
    float tx = (float)(transAmp * std::sin(2.0 * 3.141592653589793 * transFreq * tSec + transPhase));
    // Pivot around path center
    SkPoint c = {pi.bounds.centerX(), pi.bounds.centerY()};
    canvas->save();
    canvas->translate(c.x(), c.y());
    canvas->rotate((SkScalar)ang);
    canvas->translate((SkScalar)tx, 0);
    canvas->translate(-c.x(), -c.y());
    drawPathOrSprite(canvas, pi, s);
    canvas->restore();
  }
  canvas->restore();
}
