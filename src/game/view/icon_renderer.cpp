#include "icon_renderer.hpp"
#include "game_view.hpp"
#include "asset_manager.hpp"
#include "../../font.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include "include/core/SkCanvas.h"
#include "include/core/SkPathBuilder.h"
#include "include/effects/SkGradient.h"
#include "include/core/SkM44.h"
#if !defined(__EMSCRIPTEN__)
#include "piece_library.hpp"
#endif

static uint64_t nowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

static double animSeconds() {
  static int64_t base = (int64_t)nowUs();
  return (double)((int64_t)nowUs() - base) / 1e6;
}

static float getRotationAngle(float x, float y, int id, int instance, double tSec) {
  int64_t seed = (int64_t)(static_cast<int>(x)) * 73856093 ^
                 (int64_t)(static_cast<int>(y)) * 19349663 ^
                 (int64_t)id * 83492791 ^ (int64_t)instance * 2654435761;
  seed = std::llabs(seed);
  double amp = 6.0 + (double)(seed % 60) / 10.0;
  double freq = 0.3 + (double)(seed % 20) / 200.0;
  double phase = (double)(seed % 6283) / 1000.0;
  return (float)(amp * std::sin(2.0 * 3.141592653589793 * freq * tSec + phase));
}

static void lighten(const float in[3], float t, float out[3]) {
  for (int i = 0; i < 3; i++) out[i] = std::min(in[i] + (1.0f - in[i]) * t, 0.93f);
}

uint32_t IconRenderer::rgba(const float color[3], float alpha) {
  int r = (int)(color[0] * 255.0f + 0.5f);
  int g = (int)(color[1] * 255.0f + 0.5f);
  int b = (int)(color[2] * 255.0f + 0.5f);
  int a = (int)(alpha * 255.0f + 0.5f);
  if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; if (a > 255) a = 255;
  return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
}

void IconRenderer::buildIcon(GameView &view, int icon, const float color[3]) {
  GameView::IconStyle &st = view._icons[icon];
  st.glowPaint.setAntiAlias(true);
  st.symPaint.setAntiAlias(true);
  st.stemPaint.setAntiAlias(true);
  st.stemPaint.setStyle(SkPaint::kStroke_Style);
  st.stemPaint.setColor(0xFF734D1F);
  st.stemPaint.setStrokeWidth(0.22f);
  auto makeRadial = [](SkPoint c, float r, const SkColor4f cols[3], const float pos[3], SkPaint& paint) {
    SkGradient::Colors gc = SkGradient::Colors(SkSpan<const SkColor4f>(cols, 3), SkSpan<const float>(pos, 3), SkTileMode::kClamp);
    SkGradient grad(gc, SkGradient::Interpolation{});
    paint.setShader(SkShaders::RadialGradient(c, r, grad, nullptr));
  };
  {
    SkColor4f gcol[3] = { {color[0], color[1], color[2], 0.35f}, {color[0], color[1], color[2], 0.08f}, {color[0], color[1], color[2], 0.00f} };
    float gpos[3] = {0.0f, 0.6f, 1.0f};
    makeRadial(SkPoint::Make(0.0f, 0.0f), 2.2f, gcol, gpos, st.glowPaint);
  }
  SkPathBuilder sym;
  switch (icon) {
  case 1: { sym.moveTo(0.0f, 1.0f); sym.cubicTo(-1.8f, 0.1f, -1.4f, -0.9f, 0.0f, -0.35f); sym.cubicTo(1.4f, -0.9f, 1.8f, 0.1f, 0.0f, 1.0f); break; }
  case 2: { const int spikes = 5; const double outer = 1.0, inner = 0.45; for (int i = 0; i < spikes * 2; i++) { double ang = i * 3.14159265358979 / spikes - 1.57079632679489; double rad = (i % 2 == 0) ? outer : inner; double sx = std::cos(ang) * rad, sy = std::sin(ang) * rad; if (i == 0) sym.moveTo((float)sx, (float)sy); else sym.lineTo((float)sx, (float)sy); } sym.close(); break; }
  case 3: { sym.addCircle(0.0f, 0.1f, 0.82f); sym.close(); SkPathBuilder stem; stem.moveTo(0.0f, -0.65f); stem.lineTo(0.2f, -0.9f); st.stem = stem.detach(); break; }
  case 4: { sym.moveTo(0.0f, -1.0f); sym.lineTo(-1.1f, 0.9f); sym.lineTo(1.1f, 0.9f); sym.close(); break; }
  default: { sym.moveTo(0.0f, -1.0f); sym.lineTo(1.0f, 0.0f); sym.lineTo(0.0f, 1.0f); sym.lineTo(-1.0f, 0.0f); sym.close(); break; }
  }
  st.symbol = sym.detach();
  {
    float core[3]; lighten(color, 0.55f, core);
    SkColor4f scol[3] = { {core[0], core[1], core[2], 0.95f}, {color[0], color[1], color[2], 1.00f}, {color[0], color[1], color[2], 0.85f} };
    float spos[3] = {0.0f, 0.45f, 1.0f};
    makeRadial(SkPoint::Make(0.0f, -0.35f), 1.0f, scol, spos, st.symPaint);
  }
}

void IconRenderer::drawIcon(GameView &view, float cx, float cy, int icon, const float color[3], float halfW, float halfH, int instance) {
  (void)color;
  if (!view._canvas) return;
  const double W = view._targetW;
  const double H = view._targetH;
  const double px = (cx + 1.0) * 0.5 * W;
  const double py = (1.0 - cy) * 0.5 * H;
  const double rPx = std::min(halfW * W * 0.5, halfH * H * 0.5);
  if (rPx <= 0.1) return;
  view._lastVertexCount++;
#if !defined(__EMSCRIPTEN__)
  {
    const GameView::ItemRenderInfo *itemInfo = nullptr;
    auto cfgIt = view._itemRenderInfo.find(icon);
    if (cfgIt != view._itemRenderInfo.end()) itemInfo = &cfgIt->second;
    if (itemInfo && !itemInfo->svgPath.empty()) {
      SkSVGDOM *itemDom = AssetManager::getSvgDom(view, itemInfo->svgPath);
      if (itemDom) {
        view._canvas->save();
        view._canvas->resetMatrix();
        view._canvas->translate((SkScalar)px, (SkScalar)py);
        view._canvas->rotate(getRotationAngle(cx, cy, icon % GameModel::ELEMS, instance, animSeconds()));
        const float side = 2.0f * (float)rPx;
        itemDom->setContainerSize(SkSize::Make(side, side));
        view._canvas->translate(-side * 0.5f, -side * 0.5f);
        uint64_t t = nowUs();
        itemDom->render(view._canvas);
        view._bench.svgMs += (double)(nowUs() - t) / 1000.0;
        view._bench.svgN++;
        view._canvas->restore();
        return;
      }
    }
  }
#endif
  const GameView::IconStyle &st = view._icons[icon % GameModel::ELEMS];
  view._canvas->save();
  view._canvas->resetMatrix();
  view._canvas->translate((SkScalar)px, (SkScalar)py);
  float angle = getRotationAngle(cx, cy, icon % GameModel::ELEMS, instance, animSeconds());
  view._canvas->rotate(angle);
  view._canvas->scale((SkScalar)rPx, (SkScalar)rPx);
#if !defined(__EMSCRIPTEN__)
  const int ti = icon % GameModel::ELEMS;
  sk_sp<SkSVGDOM> dom = view._pieces ? view._pieces->dom(ti) : nullptr;
  if (dom) {
    if (sk_sp<SkImage> g = view._pieces->glowAt(ti)) {
      uint64_t t = nowUs();
      const float o = 1.0f;
      view._canvas->drawImageRect(g, SkRect::MakeWH((float)g->width(), (float)g->height()), SkRect::MakeXYWH(-o, -o, 2.0f * o, 2.0f * o), SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone), nullptr, SkCanvas::kFast_SrcRectConstraint);
      view._bench.glowMs += (double)(nowUs() - t) / 1000.0;
    }
    view._canvas->save();
    view._canvas->translate(-1.0f, -1.0f);
    view._canvas->scale(2.0f / PieceLib::kBase, 2.0f / PieceLib::kBase);
    dom->setContainerSize(SkSize::Make(PieceLib::kBase, PieceLib::kBase));
    uint64_t t = nowUs();
    dom->render(view._canvas);
    view._bench.svgMs += (double)(nowUs() - t) / 1000.0;
    view._bench.svgN++;
    view._canvas->restore();
  } else
#endif
  {
    uint64_t t = nowUs();
    view._canvas->drawCircle(0.0f, 0.0f, 2.2f, st.glowPaint);
    view._bench.glowMs += (double)(nowUs() - t) / 1000.0;
    view._canvas->drawPath(st.symbol, st.symPaint);
    if (!st.stem.isEmpty()) view._canvas->drawPath(st.stem, st.stemPaint);
  }
  view._canvas->restore();
}

void IconRenderer::renderCellItems(GameView &view, float centerX, float centerY, int count, const float color[3], float scale, int icon) {
  if (count <= 0) return;
  float s = scale;
  if (count < 6) s *= 1.0f + 0.6f * (6 - count) / 5.0f;
  const float rw = 0.05f * s * view._zoom;
  const float rh = 0.05f * s * view._aspect * view._zoom;
  const float sx = rw * 2.2f;
  const float sy = rh * 2.2f;
  const float hx = rw * 1.35f;
  const float vy = rh * 1.25f;
  auto put = [&](float fx, float fy, float stepX, float stepY, int inst) { drawIcon(view, centerX + fx * stepX, centerY + fy * stepY, icon, color, rw, rh, inst); };
  switch (count) {
    case 1: put(0.0f, 0.0f, sx, sy, 0); return;
    case 2: put(-0.5f, 0.0f, sx, sy, 0); put(0.5f, 0.0f, sx, sy, 1); return;
    case 3: put(0.0f, -0.5f, sx, sy, 0); put(-0.5f, 0.5f, sx, sy, 1); put(0.5f, 0.5f, sx, sy, 2); return;
    case 4: put(-0.5f, -0.5f, sx, sy, 0); put(0.5f, -0.5f, sx, sy, 1); put(-0.5f, 0.5f, sx, sy, 2); put(0.5f, 0.5f, sx, sy, 3); return;
    case 5: put(0.0f, 0.0f, sx, sy, 0); put(-0.5f, -0.5f, sx, sy, 1); put(0.5f, -0.5f, sx, sy, 2); put(-0.5f, 0.5f, sx, sy, 3); put(0.5f, 0.5f, sx, sy, 4); return;
    default: break;
  }
  int inst = 0;
  auto row = [&](float yf, const float *xs, int nxs) { for (int i = 0; i < nxs; i++) put(xs[i], yf, hx, vy, inst++); };
  if (count == 6) { static const float top[3] = {-0.75f, 0.25f, 1.25f}; static const float bot[3] = {-1.25f, -0.25f, 0.75f}; row(-0.5f, top, 3); row(0.5f, bot, 3); }
  else if (count == 7) { static const float edge[2] = {-0.5f, 0.5f}; static const float mid[3] = {-1.0f, 0.0f, 1.0f}; row(-1.0f, edge, 2); row(0.0f, mid, 3); row(1.0f, edge, 2); }
  else if (count == 8) { static const float mid[2] = {-0.5f, 0.5f}; static const float edge[3] = {-1.0f, 0.0f, 1.0f}; row(-1.0f, edge, 3); row(0.0f, mid, 2); row(1.0f, edge, 3); }
  else { static const float full[3] = {-1.0f, 0.0f, 1.0f}; row(-1.0f, full, 3); row(0.0f, full, 3); row(1.0f, full, 3); }
}
