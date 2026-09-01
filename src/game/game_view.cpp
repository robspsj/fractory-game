#include "game_view.hpp"
#include "chess_assets.h"
#include "config.hpp"
#include "particle.hpp"
#include "../font.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkM44.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkSize.h"
#include "include/core/SkSpan.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkColor.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkPixmap.h"
#if !defined(__EMSCRIPTEN__)
#include "modules/svg/include/SkSVGDOM.h"
#endif

static constexpr int MAX_PREVIEW_DEPTH = 100;

const float GameView::_elemColors[GameModel::ELEMS][3] = {
    {0.15f, 0.50f, 0.90f}, {0.90f, 0.30f, 0.30f}, {0.20f, 0.80f, 0.35f},
    {0.95f, 0.85f, 0.15f}, {0.80f, 0.30f, 0.85f},
};

const float GameView::_white[3] = {1.0f, 1.0f, 1.0f};
const float GameView::_yellow[3] = {1.0f, 1.0f, 0.0f};
const float GameView::_grey[3] = {0.5f, 0.5f, 0.5f};
const float GameView::_gridBg[3] = {0.25f, 0.40f, 0.60f};
const float GameView::_stationColor[3] = {0.6f, 0.4f, 0.2f};
const float GameView::_reservedInput[3] = {0.2f, 0.6f, 0.3f};
const float GameView::_reservedOutput[3] = {0.6f, 0.2f, 0.2f};
const float GameView::_reservedBuffer[3] = {0.4f, 0.4f, 0.4f};

static uint64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Microsecond-res time for per-stage frame-budget timing (glow / SVG render).
static uint64_t nowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Smooth, low-magnitude animation clock. Returns seconds since the first call
// (i.e. since app launch), measured in MICROseconds. Two advantages over the raw
// wall-clock nowMs(): (1) the value stays small (a few hours => ~1e4) so the sine
// argument keeps full double precision instead of landing near ~1e7 rad where
// large-argument sin can flatten between frames; (2) microsecond resolution means
// the wobble advances every frame instead of quantising to whole milliseconds
// (which, at 60+ fps, made some frames repeat the same angle -> "no animation").
static double animSeconds() {
  static int64_t base = (int64_t)nowUs();
  return (double)((int64_t)nowUs() - base) / 1e6;
}

// Deterministic per-instance rotation angle (degrees): pure function of the
// icon's scene position, element id, per-copy instance index and animation time.
// No per-item state is stored anywhere — same (x,y,id,instance,t) => same angle.
// The index (0..count-1) distinguishes multiple copies of the same element in a
// cell, so each copy rocks on its own even when positions are near-identical.
// Hash is computed in int64 to avoid signed int overflow (UB).
static float getRotationAngle(float x, float y, int id, int instance, double tSec) {
  int64_t seed = (int64_t)(static_cast<int>(x)) * 73856093 ^
                 (int64_t)(static_cast<int>(y)) * 19349663 ^
                 (int64_t)id * 83492791 ^ (int64_t)instance * 2654435761;
  seed = std::llabs(seed);
  double amp = 6.0 + (double)(seed % 60) / 10.0;     // 6..11.9 deg swing
  double freq = 0.3 + (double)(seed % 20) / 200.0;    // 0.3..0.4 Hz (tight, consistent)
  double phase = (double)(seed % 6283) / 1000.0;     // 0..2pi rad, de-syncs neighbours
  return (float)(amp * std::sin(2.0 * 3.141592653589793 * freq * tSec + phase));
}

GameView::GameView(GameModel &model, const Config &cfg)
    : _model(model), _showAnchor(cfg.showAnchor) {
  // Load the item-render CSV automatically at startup.
  loadItemConfig();

  const Cell &cell = _model.node(_anchorIndex);
  _anchorSize =
      (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    _anchorDepth++;
  // Pre-cache the vector-symbol styles (paths + radial gradients) once, so per
  // frame we only place them via canvas transforms.
  for (int i = 0; i < GameModel::ELEMS; i++)
    buildIcon(i, _elemColors[i]);
}

// Blend a color toward white by t in [0,1], clamped so it never hits pure
// 255 for any channel (avoids white flashes when an icon rasterizes to 1-2px).
static void lighten(const float in[3], float t, float out[3]) {
  for (int i = 0; i < 3; i++)
    out[i] = std::min(in[i] + (1.0f - in[i]) * t, 0.93f);
}

uint32_t GameView::rgba(const float color[3], float alpha) {
  int r = (int)(color[0] * 255.0f + 0.5f);
  int g = (int)(color[1] * 255.0f + 0.5f);
  int b = (int)(color[2] * 255.0f + 0.5f);
  int a = (int)(alpha * 255.0f + 0.5f);
  if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; if (a > 255) a = 255;
  return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
}

// Build the cached icon style for element `icon` (0..ELEMS-1). Shapes and
// gradients are defined in UNIT space centred on the origin with radius ~1,
// then placed at any on-screen radius/position via canvas translate+scale.
void GameView::buildIcon(int icon, const float color[3]) {
  IconStyle &st = _icons[icon];
  st.glowPaint.setAntiAlias(true);
  st.symPaint.setAntiAlias(true);
  st.stemPaint.setAntiAlias(true);
  st.stemPaint.setStyle(SkPaint::kStroke_Style);
  // apple stem colour (ARGB). 0x73,0x4D,0x1F = {0.45,0.30,0.12}
  st.stemPaint.setColor(0xFF734D1F);
  st.stemPaint.setStrokeWidth(0.22f);  // unit width (scaled by radius at draw)

  // --- soft glow disc: radial gradient fading out to radius 2.2 (unit) ---
  auto makeRadial = [](SkPoint c, float r, const SkColor4f cols[3],
                       const float pos[3], SkPaint& paint) {
    SkGradient::Colors gc =
        SkGradient::Colors(SkSpan<const SkColor4f>(cols, 3), SkSpan<const float>(pos, 3),
                           SkTileMode::kClamp);
    SkGradient grad(gc, SkGradient::Interpolation{});
    paint.setShader(SkShaders::RadialGradient(c, r, grad, nullptr));
  };
  {
    SkColor4f gcol[3] = {
        {color[0], color[1], color[2], 0.35f},
        {color[0], color[1], color[2], 0.08f},
        {color[0], color[1], color[2], 0.00f},
    };
    float gpos[3] = {0.0f, 0.6f, 1.0f};
    makeRadial(SkPoint::Make(0.0f, 0.0f), 2.2f, gcol, gpos, st.glowPaint);
  }

  // --- symbol shape (unit coords), built via SkPathBuilder then detached ---
  SkPathBuilder sym;
  switch (icon) {
  case 1: { // Heart (point down)
    sym.moveTo(0.0f, 1.0f);
    sym.cubicTo(-1.8f, 0.1f, -1.4f, -0.9f, 0.0f, -0.35f);
    sym.cubicTo(1.4f, -0.9f, 1.8f, 0.1f, 0.0f, 1.0f);
    break;
  }
  case 2: { // Star (5-point)
    const int spikes = 5;
    const double outer = 1.0, inner = 0.45;
    for (int i = 0; i < spikes * 2; i++) {
      double ang = i * 3.14159265358979 / spikes - 1.57079632679489;
      double rad = (i % 2 == 0) ? outer : inner;
      double sx = std::cos(ang) * rad, sy = std::sin(ang) * rad;
      if (i == 0) sym.moveTo((float)sx, (float)sy); else sym.lineTo((float)sx, (float)sy);
    }
    sym.close();
    break;
  }
  case 3: { // Apple: rounded body + brown-ish stem stub
    sym.addCircle(0.0f, 0.1f, 0.82f);
    sym.close();
    SkPathBuilder stem;
    stem.moveTo(0.0f, -0.65f);
    stem.lineTo(0.2f, -0.9f);
    st.stem = stem.detach();
    break;
  }
  case 4: { // Triangle (equilateral, up)
    sym.moveTo(0.0f, -1.0f);
    sym.lineTo(-1.1f, 0.9f);
    sym.lineTo(1.1f, 0.9f);
    sym.close();
    break;
  }
  default: { // Diamond
    sym.moveTo(0.0f, -1.0f);
    sym.lineTo(1.0f, 0.0f);
    sym.lineTo(0.0f, 1.0f);
    sym.lineTo(-1.0f, 0.0f);
    sym.close();
    break;
  }
  }
  st.symbol = sym.detach();

  // --- symbol fill: radial highlight (bright core -> element color) ---
  {
    float core[3];
    lighten(color, 0.55f, core);  // never pure white => no flash on tiny icons
    SkColor4f scol[3] = {
        {core[0], core[1], core[2], 0.95f},
        {color[0], color[1], color[2], 1.00f},
        {color[0], color[1], color[2], 0.85f},
    };
    // gradient centred slightly above the symbol for a polished highlight
    float spos[3] = {0.0f, 0.45f, 1.0f};
    makeRadial(SkPoint::Make(0.0f, -0.35f), 1.0f, scol, spos, st.symPaint);
  }
}

// Owns the parsed chess-piece SVGDOMs and a one-slot-per-element raster cache,
// re-rendering each piece from the vector only when its on-screen pixel size
// changes — so items stay crisp at any zoom (no fixed-PNG upscaling).
#if !defined(__EMSCRIPTEN__)
// Holds the parsed chess-piece SVGDOMs plus a baked soft glow sprite per element.
// Pieces are rendered from the vector (SkSVGDOM::render) directly at each
// instance's position/scale every frame — crisp at any zoom, no raster cache.
// The glow is a blurred, element-tinted silhouette (follows the piece contour,
// far less area than a full radial disc) pre-baked once at modest resolution:
// because it is inherently soft there's no crispness to lose by scaling it.
struct PieceLib {
  static constexpr float kBase = 72.0f;      // SVG viewBox; maps to the icon box
  static constexpr int   kGlowBase = 128;    // glow sprite res (soft → fixed is fine)
  static constexpr float kGlowSigma = 5.5f;  // blur spread (px, at kGlowBase)
  std::array<sk_sp<SkSVGDOM>, GameModel::ELEMS> doms;
  std::array<sk_sp<SkImage>, GameModel::ELEMS> glow;

  explicit PieceLib() {
    for (int i = 0; i < (int)GameModel::ELEMS; i++)
      glow[i] = nullptr;
    for (int i = 0; i < (int)GameModel::ELEMS; i++) {
      const chess::Svg& sv = chess::pieces[i % (int)GameModel::ELEMS];
      SkMemoryStream stream(sv.data, sv.size, false);
      doms[i] = SkSVGDOM::MakeFromStream(stream);
      if (doms[i])
        doms[i]->setContainerSize(SkSize::Make(kBase, kBase));
    }
  }

  sk_sp<SkSVGDOM> dom(int idx) const {
    return (idx >= 0 && idx < (int)GameModel::ELEMS) ? doms[idx] : nullptr;
  }
  sk_sp<SkImage> glowAt(int idx) const {
    return (idx >= 0 && idx < (int)GameModel::ELEMS) ? glow[idx] : nullptr;
  }

  // Bake a coloured OUTLINE glow for piece `idx`: spread the piece silhouette
  // outward with a gaussian blur, then subtract the crisp silhouette so only the
  // outer blurred RIM remains — an "extra outline" whose blur falls outside the
  // contour. Much tighter than a filled halo; hugs the piece shape. The crisp
  // piece itself stays vector per-instance.
  void buildGlow(int idx, const float* color) {
    if (idx < 0 || idx >= (int)GameModel::ELEMS || !doms[idx])
      return;
    const int S = kGlowBase;
    SkImageInfo info = SkImageInfo::Make(S, S, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> pieceSurf = SkSurfaces::Raster(info);
    if (!pieceSurf)
      return;
    doms[idx]->setContainerSize(SkSize::Make(S, S));
    doms[idx]->render(pieceSurf->getCanvas());
    sk_sp<SkImage> piece = pieceSurf->makeImageSnapshot();
    // Outward blur of the silhouette (spreads the edge a few px past the contour).
    SkPaint blur;
    blur.setImageFilter(SkImageFilters::Blur(kGlowSigma, kGlowSigma, SkTileMode::kClamp,
                                             nullptr));
    sk_sp<SkSurface> blurredSurf = SkSurfaces::Raster(info);
    blurredSurf->getCanvas()->clear(SkColors::kTransparent);
    blurredSurf->getCanvas()->drawImage(piece, 0.0f, 0.0f, SkSamplingOptions(), &blur);
    sk_sp<SkImage> blurred = blurredSurf->makeImageSnapshot();
    // Rim alpha = blurredAlpha - crispAlpha (>=0): only the outer blurred edge.
    std::vector<uint8_t> buf((size_t)S * S * 4);
    std::vector<uint8_t> tmp((size_t)S * S * 4);
    std::vector<uint8_t> crispA((size_t)S * S);
    piece->readPixels(info, tmp.data(), info.minRowBytes(), 0, 0);
    for (size_t i = 0; i < (size_t)S * S; i++)
      crispA[i] = tmp[i * 4 + 3];
    blurred->readPixels(info, tmp.data(), info.minRowBytes(), 0, 0);
    const int er = (int)(color[0] * 255.f), eg = (int)(color[1] * 255.f), eb = (int)(color[2] * 255.f);
    for (size_t i = 0; i < (size_t)S * S; i++) {
      int a = (int)tmp[i * 4 + 3] - (int)crispA[i];
      if (a < 0) a = 0;
      buf[i * 4 + 0] = (uint8_t)((er * a) / 255);   // premultiplied element colour
      buf[i * 4 + 1] = (uint8_t)((eg * a) / 255);
      buf[i * 4 + 2] = (uint8_t)((eb * a) / 255);
      buf[i * 4 + 3] = (uint8_t)a;
    }
    glow[idx] = [&]() -> sk_sp<SkImage> {
      sk_sp<SkSurface> gp = SkSurfaces::Raster(info);
      if (!gp)
        return nullptr;
      gp->writePixels(SkPixmap(info, buf.data(), info.minRowBytes()), 0, 0);
      return gp->makeImageSnapshot();
    }();
  }
};

// Rounded-color-square tiles (assets/ui/*.svg) drawn per grid/cell background.
// Each tile is an independent .svg loaded at runtime via SkSVGDOM and rendered
// per cell — no raster cache, no generated header. `bg[i]` = grid container,
// `cell[i]` = inner cell, where i = GridData.variation (a per-size colour family).
#if !defined(__EMSCRIPTEN__)
struct UiLib {
  static constexpr int kFamilies = 7;
  std::array<sk_sp<SkSVGDOM>, kFamilies> bg;
  std::array<sk_sp<SkSVGDOM>, kFamilies> cell;
  explicit UiLib() {
    for (int i = 0; i < kFamilies; i++) {
      char p[64];
      snprintf(p, sizeof(p), "assets/ui/bg_%d.svg", i);
      bg[i] = load(p);
      snprintf(p, sizeof(p), "assets/ui/cell_%d.svg", i);
      cell[i] = load(p);
    }
  }
  // Parse an independent .svg file from disk into an SkSVGDOM. Returns nullptr
  // (graceful flat-quad fallback) if the file is missing/unreadable.
  static sk_sp<SkSVGDOM> load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return nullptr; }
    std::vector<char> buf((size_t)n);
    bool ok = fread(buf.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok) return nullptr;
    SkMemoryStream stream(buf.data(), (size_t)n, false);
    return SkSVGDOM::MakeFromStream(stream);
  }
};

// Render an SVG tile into the NDC rect `r` (viewBox maps to the rect). The canvas
// is under the renderer's NDC matrix, so the tile lands exactly where a flat
// quad would have.
static void renderTile(SkCanvas *canvas, const sk_sp<SkSVGDOM> &dom, Rect r) {
  if (!dom || !canvas) return;
  if (r.outsideClip() || r.w < 0.002f || r.h < 0.002f) return;
  dom->setContainerSize(SkSize::Make(r.w, r.h));
  canvas->save();
  canvas->translate((SkScalar)r.ox, (SkScalar)r.oy);
  dom->render(canvas);
  canvas->restore();
}
#endif

void GameView::loadChessAssets() {
  delete _pieces;
  _pieces = new PieceLib();
  int ok = 0;
  for (int i = 0; i < (int)GameModel::ELEMS; i++) {
    if (_pieces->doms[i]) { ok++; _pieces->buildGlow(i, _elemColors[i]); }
  }
  fprintf(stderr, "loadChessAssets: %d/%d piece SVGs parsed (glow baked)\n", ok,
          (int)GameModel::ELEMS);
}
#endif

#if !defined(__EMSCRIPTEN__)
void GameView::loadUiAssets() {
  delete _ui;
  _ui = new UiLib();
  int ok = 0;
  for (int i = 0; i < UiLib::kFamilies; i++)
    if (_ui->bg[i] && _ui->cell[i]) ok++;
  fprintf(stderr, "loadUiAssets: %d/%d tile SVGs parsed\n", ok, (int)UiLib::kFamilies);
}
#endif

GameView::~GameView() {
#if !defined(__EMSCRIPTEN__)
  delete _pieces;
  delete _ui;
#endif
}

void GameView::addQuad(Rect const r, const float color[3], float alpha) {
  if (r.outsideClip())
    return;
  if (r.w < 0.002f || r.h < 0.002f)
    return;
  if (!_canvas)
    return;
  _lastVertexCount++;
  // rect is in NDC; the context transform maps it to pixels (y-up -> y-down).
  _quadPaint.setColor(rgba(color, alpha));
  _quadPaint.setAntiAlias(true);
  _canvas->drawRect(SkRect::MakeXYWH(r.ox, r.oy, r.w, r.h), _quadPaint);
}

// Draw a vector symbol filled with a radial gradient, plus a soft glow disc
// behind it. Drawn in PIXEL space (matrix reset) so the gradient is a true
// circle in screen space — a NDC-space gradient would become an ellipse
// (x scaled by w/2, y by h/2), warping with window size. cx/cy/halfW/halfH in NDC.
void GameView::drawIcon(float cx, float cy, int icon, const float color[3],
                        float halfW, float halfH, int instance) {
  (void)color;  // colour is baked into the cached style per element
  if (!_canvas)
    return;
  // NDC -> pixel. targetWidth/Height are the image pixel size.
  const double W = _targetW;
  const double H = _targetH;
  const double px = (cx + 1.0) * 0.5 * W;   // x
  const double py = (1.0 - cy) * 0.5 * H;   // y (NDC y-up -> pixel y-down)
  // Pixel radius: keep it circular in screen space.
  const double rPx = std::min(halfW * W * 0.5, halfH * H * 0.5);
  if (rPx <= 0.1)
    return;
  _lastVertexCount++;

#if !defined(__EMSCRIPTEN__)
  // Data-driven item icon: `icon` carries the item id at every call site
  // (renderItem, the drag preview, reserved cells). Look up the item's render
  // config and draw its SVG per-item — right-side-up and aspect-independent, in
  // pixel space. Native-only (web has no expat/SkSVGDOM and stays on symbols).
  {
    const ItemRenderInfo *itemInfo = nullptr;
    auto cfgIt = _itemRenderInfo.find(icon);
    if (cfgIt != _itemRenderInfo.end()) itemInfo = &cfgIt->second;
    if (itemInfo && !itemInfo->svgPath.empty()) {
      SkSVGDOM *itemDom = getSvgDom(itemInfo->svgPath);
      if (itemDom) {
        _canvas->save();
        _canvas->resetMatrix();
        _canvas->translate((SkScalar)px, (SkScalar)py);
        _canvas->rotate(getRotationAngle(cx, cy, icon % GameModel::ELEMS, instance,
                                         animSeconds()));
        const float side = 2.0f * (float)rPx;
        itemDom->setContainerSize(SkSize::Make(side, side));
        _canvas->translate(-side * 0.5f, -side * 0.5f);
        uint64_t t = nowUs();
        itemDom->render(_canvas);
        _bench.svgMs += (double)(nowUs() - t) / 1000.0;
        _bench.svgN++;
        _canvas->restore();
        return;
      }
    }
  }
#endif

  const IconStyle &st = _icons[icon % GameModel::ELEMS];
  // Work in pixel space: reset to identity, position at (px,py), scale by the
  // on-screen radius so the symbol/glow scale with zoom.
  _canvas->save();
  _canvas->resetMatrix();
  _canvas->translate((SkScalar)px, (SkScalar)py);
  // Independently rotate each icon on the screen plane. Both the glow and the
  // crisp piece share the same transform, so they spin together around the
  // icon's centre. Pure function of scene pos + id + time => no logic state.
  float angle = getRotationAngle(cx, cy, icon % GameModel::ELEMS, instance, animSeconds());
  _canvas->rotate(angle);
  _canvas->scale((SkScalar)rPx, (SkScalar)rPx);
#if !defined(__EMSCRIPTEN__)
  const int ti = icon % GameModel::ELEMS;
  sk_sp<SkSVGDOM> dom = _pieces ? _pieces->dom(ti) : nullptr;
  if (dom) {
    // Glow: baked blurred + element-tinted silhouette (follows the piece contour,
    // far less area than a radial disc and inherently soft, so a scaled fixed
    // sprite is fine). Drawn behind the crisp piece.
    if (sk_sp<SkImage> g = _pieces->glowAt(ti)) {
      uint64_t t = nowUs();
      const float o = 1.0f;   // draw the outline glow at the piece's box; its outward
                                      // blur is baked in, so it hugs the contour
      _canvas->drawImageRect(g, SkRect::MakeWH((float)g->width(), (float)g->height()),
                             SkRect::MakeXYWH(-o, -o, 2.0f * o, 2.0f * o),
                             SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone),
                             nullptr, SkCanvas::kFast_SrcRectConstraint);
      _bench.glowMs += (double)(nowUs() - t) / 1000.0;
    }
    // Crisp vector piece, rendered from the SVG per instance at its on-screen box
    // (no cached bitmap; the SVG geometry is centred in the 72 viewBox). Map the
    // container plane [0,kBase]^2 onto the unit box [-1,1]^2.
    _canvas->save();
    _canvas->translate(-1.0f, -1.0f);
    _canvas->scale(2.0f / PieceLib::kBase, 2.0f / PieceLib::kBase);
    // CRITICAL: buildGlow() changes the DOM's container to kGlowBase for the sprite
    // and doesn't restore it — force kBase here so [0,kBase] maps to [-1,1]. Without
    // this the piece renders scaled 128/72 and drifts to the bottom-right.
    dom->setContainerSize(SkSize::Make(PieceLib::kBase, PieceLib::kBase));
    uint64_t t = nowUs();
    dom->render(_canvas);
    _bench.svgMs += (double)(nowUs() - t) / 1000.0;
    _bench.svgN++;
    _canvas->restore();
  } else
#endif
  {
    // Fallback / web: vector symbol with a soft radial glow behind it.
    uint64_t t = nowUs();
    _canvas->drawCircle(0.0f, 0.0f, 2.2f, st.glowPaint);
    _bench.glowMs += (double)(nowUs() - t) / 1000.0;
    _canvas->drawPath(st.symbol, st.symPaint);
    if (!st.stem.isEmpty())
      _canvas->drawPath(st.stem, st.stemPaint);
  }
  _canvas->restore();
}

void GameView::loadItemConfig(const std::string &path) {
  _itemRenderInfo.clear();
  std::string tryPath = path;
  if (!std::filesystem::exists(tryPath)) {
    tryPath = "../" + path;
  }
  std::ifstream in(tryPath);
  if (!in) {
    std::cout << "[GameView] WARNING: could not open item config \"" << tryPath << "\"" << std::endl;
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    // trim leading whitespace
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) continue;
    line = line.substr(start);
    if (line.empty() || line[0] == '#') continue;

    // Split on commas (the last field may contain spaces, e.g. SVG file names
    // like "assets/Pétanque Cochonnet.svg"). Splitting on commas (rather than
    // whitespace) keeps those paths intact.
    std::vector<std::string> f;
    size_t pos = 0;
    while (pos <= line.size()) {
      size_t comma = line.find(',', pos);
      std::string tok = (comma == std::string::npos) ? line.substr(pos)
                                                     : line.substr(pos, comma - pos);
      size_t a = tok.find_first_not_of(" \t");
      size_t b = tok.find_last_not_of(" \t");
      tok = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
      f.push_back(tok);
      if (comma == std::string::npos) break;
      pos = comma + 1;
    }
    if (f.size() < 6) continue;

    try {
      int id = std::stoi(f[0]);
      float r = std::stof(f[2]), g = std::stof(f[3]), b = std::stof(f[4]);
      int icon = std::stoi(f[5]);
      ItemRenderInfo info;
      info.color[0] = r; info.color[1] = g; info.color[2] = b;
      info.icon = icon;
      info.svgPath = (f.size() > 6) ? f[6] : "";
      _itemRenderInfo.emplace(id, std::move(info));
    } catch (...) {
      continue;  // skip malformed rows rather than aborting the whole load
    }
  }
  std::cout << "[GameView] Loaded " << _itemRenderInfo.size() << " item configs from \""
            << tryPath << "\"" << std::endl;
}

void GameView::debugIcon(SkCanvas* canvas, int w, int h, int icon, float rFrac) {
  _canvas = canvas;
  _targetW = w;
  _targetH = h;
  _aspect = (float)w / (float)h;
  const int ti = icon % GameModel::ELEMS;
  drawIcon(0.0f, 0.0f, ti, _elemColors[ti], rFrac, rFrac);
  _canvas = nullptr;
}

SkSVGDOM* GameView::getSvgDom(const std::string &path) {
  if (path.empty()) return nullptr;
  auto it = _svgCache.find(path);
  if (it != _svgCache.end()) return it->second.get();
  sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
  if (!data) {
    // CWD-agnostic fallback: if the game was launched from build-native/, the
    // asset lives one level up. Mirror loadItemConfig's ../ retry.
    std::string alt = "../" + path;
    if (std::filesystem::exists(alt))
      data = SkData::MakeFromFileName(alt.c_str());
  }
  if (!data) return nullptr;
  SkMemoryStream stream(data);
  sk_sp<SkSVGDOM> dom = SkSVGDOM::MakeFromStream(stream);
  if (!dom) return nullptr;
  SkSVGDOM* raw = dom.get();
  _svgCache[path] = std::unique_ptr<SkSVGDOM>(dom.release());
  return raw;
}

void GameView::renderCellItems(float centerX, float centerY, int count,
                               const float color[3], float scale, int icon) {
  if (count <= 0)
    return;
  // Prominence scaling: a lone item is drawn biggest, and the boost eases
  // off linearly as the stack grows until it reaches exactly 1x at 6 items.
  // 7+ items stop scaling entirely — they fill the cell by count.
  float s = scale;
  if (count < 6)
    s *= 1.0f + 0.6f * (6 - count) / 5.0f;  // count 1: 1.6x ... count 5: 1.12x
  const float rw = 0.05f * s * _zoom;           // icon half-size (NDC x)
  const float rh = 0.05f * s * _aspect * _zoom; // icon half-size (NDC y)

  // Centre-to-centre spacing: tight enough that a stack reads as one bunch.
  const float sx = rw * 2.2f;   // readable grid for 1..5
  const float sy = rh * 2.2f;
  // Dense hex-style pile for 6..9: row step and vertical row spacing.
  const float hx = rw * 1.35f;
  const float vy = rh * 1.25f;

  auto put = [&](float fx, float fy, float stepX, float stepY, int inst) {
    drawIcon(centerX + fx * stepX, centerY + fy * stepY, icon, color, rw, rh, inst);
  };

  // Readable stacking strategies for 1..5 — every icon stays fully visible.
  switch (count) {
    case 1:
      put(0.0f, 0.0f, sx, sy, 0);
      return;
    case 2:
      put(-0.5f, 0.0f, sx, sy, 0);
      put(0.5f, 0.0f, sx, sy, 1);
      return;
    case 3:
      put(0.0f, -0.5f, sx, sy, 0);
      put(-0.5f, 0.5f, sx, sy, 1);
      put(0.5f, 0.5f, sx, sy, 2);
      return;
    case 4:
      put(-0.5f, -0.5f, sx, sy, 0);
      put(0.5f, -0.5f, sx, sy, 1);
      put(-0.5f, 0.5f, sx, sy, 2);
      put(0.5f, 0.5f, sx, sy, 3);
      return;
    case 5:
      put(0.0f, 0.0f, sx, sy, 0);
      put(-0.5f, -0.5f, sx, sy, 1);
      put(0.5f, -0.5f, sx, sy, 2);
      put(-0.5f, 0.5f, sx, sy, 3);
      put(0.5f, 0.5f, sx, sy, 4);
      return;
    default:
      break;
  }

  // Dense hex-packed piles for 6..9: half-step row offsets so the items
  // nestle together — 3/3, 2/3/2 (flower), 3/2/3, 3/3/3. 10+ cap at the 9
  // layout; the count badge already shows the real number.
  int inst = 0;
  auto row = [&](float yf, const float *xs, int nxs) {
    for (int i = 0; i < nxs; i++)
      put(xs[i], yf, hx, vy, inst++);
  };
  if (count == 6) {
    static const float top[3] = {-0.75f, 0.25f, 1.25f};
    static const float bot[3] = {-1.25f, -0.25f, 0.75f};
    row(-0.5f, top, 3);
    row(0.5f, bot, 3);
  } else if (count == 7) {
    static const float edge[2] = {-0.5f, 0.5f};
    static const float mid[3] = {-1.0f, 0.0f, 1.0f};
    row(-1.0f, edge, 2);
    row(0.0f, mid, 3);
    row(1.0f, edge, 2);
  } else if (count == 8) {
    static const float mid[2] = {-0.5f, 0.5f};
    static const float edge[3] = {-1.0f, 0.0f, 1.0f};
    row(-1.0f, edge, 3);
    row(0.0f, mid, 2);
    row(1.0f, edge, 3);
  } else {
    static const float full[3] = {-1.0f, 0.0f, 1.0f};
    row(-1.0f, full, 3);
    row(0.0f, full, 3);
    row(1.0f, full, 3);
  }
}

void GameView::renderEmpty(Rect r, const float bgColor[3]) {
  const float *col = bgColor ? bgColor : _grey;
  addQuad(r, col);
}

void GameView::renderItem(Rect r, int itemId, int count, float scale,
                          const float bgColor[3], int variation) {
  // 1. Background: Restore the detailed UI-based tiles (variation-driven)
#if !defined(__EMSCRIPTEN__)
  if (_ui) {
    int vi = std::clamp(variation - 1, 0, UiLib::kFamilies - 1);
    if (_ui->cell[vi]) renderTile(_canvas, _ui->cell[vi], r);
  } else
#endif
  {
    const float *bgCol = bgColor ? bgColor : _grey;
    addQuad(r, bgCol);
  }

  // 2. Look up the item render configuration (colour/icon fallback only — the
  // actual SVG is resolved per-icon inside drawIcon, keyed by the item id).
  const ItemRenderInfo *info = nullptr;
  auto it = _itemRenderInfo.find(itemId);
  if (it != _itemRenderInfo.end()) {
    info = &it->second;
  } else if (itemId >= 0 && itemId < GameModel::ELEMS) {
    const float *legacyCol = _elemColors[itemId];
    static ItemRenderInfo tmp;
    tmp.color[0] = legacyCol[0]; tmp.color[1] = legacyCol[1]; tmp.color[2] = legacyCol[2];
    tmp.icon = itemId % GameModel::ELEMS;
    tmp.svgPath.clear();
    info = &tmp;
  }

  // 3. Render the items — each item instance is drawn by drawIcon, which uses
  // the item's SVG (or vector-symbol fallback). Pass the item id so multiple
  // copies in a cell each render their own icon (and the drag preview matches).
  const float *elemCol = info ? info->color : _white;
  renderCellItems(r.cx(), r.cy(), count, elemCol, scale, itemId);

  // Large stacks (5+): draw a compact "X{count}" badge at the cell's
  // bottom-right corner so the true quantity stays visible without drawing
  // dozens of icons.
  if (count >= 5)
    drawCountBadge(r, count);
}

void GameView::drawCountBadge(const Rect &r, int count) {
  if (!_canvas)
    return;
  // Cell size in pixels; skip the badge when the cell is too small to read it.
  const float pw = r.w * (float)_targetW * 0.5f;
  const float ph = r.h * (float)_targetH * 0.5f;
  const float cellMin = std::min(pw, ph);
  if (cellMin < 18.0f)
    return;

  char buf[16];
  snprintf(buf, sizeof(buf), "X%d", count);

  // Glyph scale so the badge stays a modest fraction of the cell, clamped to
  // stay readable at high zoom without dominating small cells.
  const float scale = std::clamp(cellMin / 48.0f, 1.0f, 6.0f);
  const float textW = textWidth(buf, scale);
  const float textH = textHeight(scale);

  // Cell bounds in pixels (NDC y-up -> pixel y-down).
  const float cellLeft = (r.ox + 1.0f) * 0.5f * (float)_targetW;
  const float cellRight = (r.ox + r.w + 1.0f) * 0.5f * (float)_targetW;
  const float cellBottom = (1.0f - r.oy) * 0.5f * (float)_targetH;
  const float cellTop = (1.0f - (r.oy + r.h)) * 0.5f * (float)_targetH;

  // Pill size and padding.
  const float padX = 3.0f, padY = 2.0f;
  const float pillW = textW + padX * 2.0f;
  const float pillH = textH + padY * 2.0f;

  // Anchor pill at bottom-right, but clamp so it stays inside the cell.
  float pillX = cellRight - pillW;
  float pillY = cellBottom - pillH;
  if (pillX < cellLeft) pillX = cellLeft;
  if (pillY < cellTop) pillY = cellTop;

  _canvas->save();
  _canvas->resetMatrix();  // pixel space for the vector font
  drawText(_canvas, pillX + padX, pillY + padY, buf, scale);
  _canvas->restore();
}

Rect GameView::childCellLayout(int nodeIndex, Rect r, int row, int col) const {
  const Cell& cell = _model.node(nodeIndex);
  int gridDim = cell.content.data.grid.gridDimension;

  float childW = r.w / (gridDim + (gridDim - 1) * _gapRatio);
  float childH = r.h / (gridDim + (gridDim - 1) * _gapRatio);
  float pitchX = childW * (1 + _gapRatio);
  float pitchY = childH * (1 + _gapRatio);
  float halfW = childW * 0.5f;
  float halfH = childH * 0.5f;

  float centerX = r.ox + halfW + col * pitchX;
  float centerY = r.oy + r.h - halfH - row * pitchY;
  return {centerX - halfW, centerY - halfH, childW, childH};
}

void GameView::renderGrid(int nodeIndex, Rect r, int depth, int excludeChild) {
  const Cell &cell = _model.node(nodeIndex);
  int firstChild = cell.content.data.grid.firstChild;
  int gridDim = cell.content.data.grid.gridDimension;

  int absDepth = 0;
  for (int i = nodeIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    absDepth++;
  float grayValue = 0.45f - std::min(absDepth, 10) * 0.03f;
  const float gridBg[3] = {grayValue, grayValue, grayValue + 0.03f};
#if !defined(__EMSCRIPTEN__)
  if (_ui) {
    int vi = std::clamp(cell.content.data.grid.variation - 1, 0, UiLib::kFamilies - 1);
    if (_ui->bg[vi]) renderTile(_canvas, _ui->bg[vi], r);
  } else
#endif
  addQuad(r, (_showAnchor && depth == 0) ? _gridBg : gridBg);

  for (int row = 0; row < gridDim; row++) {
    for (int col = 0; col < gridDim; col++) {
      int childIndex = firstChild + row * gridDim + col;
      if (childIndex == excludeChild)
        continue;
      Rect child = childCellLayout(nodeIndex, r, row, col);
      renderCell(childIndex, child, depth + 1);
    }
  }
}

void GameView::renderCell(int nodeIndex, Rect r, int depth) {
  int absDepth = 0;
  for (int i = nodeIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    absDepth++;
  if (absDepth >= MAX_PREVIEW_DEPTH)
    return;
  if (r.outsideClip())
    return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize)
    return;

  const Cell &cell = _model.node(nodeIndex);
  const bool isAnchor = _showAnchor && nodeIndex == _anchorIndex;
  switch (cell.content.type) {
  case CellType::EMPTY:
#if !defined(__EMSCRIPTEN__)
    if (_ui) {
      int vi = std::clamp(variationOf(nodeIndex) - 1, 0, UiLib::kFamilies - 1);
      if (_ui->cell[vi]) renderTile(_canvas, _ui->cell[vi], r);
      else renderEmpty(r, isAnchor ? _gridBg : nullptr);
    } else
#endif
    renderEmpty(r, isAnchor ? _gridBg : nullptr);
    break;
  case CellType::ITEM:
  { float scale = r.w / (_zoom * _cellSize);
    renderItem(r, cell.content.data.item.id, cell.content.data.item.count,
               scale, isAnchor ? _gridBg : nullptr, variationOf(nodeIndex));
    break;
  }
  case CellType::GRID:
    renderGrid(nodeIndex, r, depth);
    break;
  case CellType::STATION:
    addQuad(r, _stationColor);
    break;
  case CellType::RESERVED: {
    const ReservedData &rd = cell.content.data.reserved;
    const float *col = _grey;
    switch (rd.role) {
      case ReserveRole::INPUT:  col = _reservedInput; break;
      case ReserveRole::OUTPUT: col = _reservedOutput; break;
      case ReserveRole::BUFFER: col = _reservedBuffer; break;
      default: break;
    }
    addQuad(r, col);
    if (rd.itemId >= 0) {
      const float *elemCol =
          (rd.itemId >= 0 && rd.itemId < GameModel::ELEMS) ? _elemColors[rd.itemId] : _white;
      renderCellItems(r.cx(), r.cy(), rd.itemCount, elemCol, 1.0f, rd.itemId);
    }
    break;
  }
  }
}

int GameView::resolveLeafCell(float worldX, float worldY) const {
  constexpr float anchorW = _anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  int leafIdx = -1;

  if (r.contains(worldX, worldY)) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.content.type != CellType::GRID) leafIdx = _anchorIndex;
    else leafIdx = resolveCellAt(worldX, worldY, _anchorIndex, _anchorSize, r);
  } else {
    int currIdx = _anchorIndex;
    Rect currR = r;

    while (true) {
      int parentIdx = _model.node(currIdx).parentId;
      if (parentIdx < 0) break;

      const Cell &parent = _model.node(parentIdx);
      if (parent.content.type != CellType::GRID) break;

      int parentDim = parent.content.data.grid.gridDimension;
      int firstChild = parent.content.data.grid.firstChild;
      int offset = currIdx - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;
      if (row < 0 || row >= parentDim || col < 0 || col >= parentDim) break;

      float k = (float)(parentDim + (parentDim - 1) * _gapRatio);
      float pitchX = currR.w * (1 + _gapRatio);
      float pitchY = currR.h * (1 + _gapRatio);
      Rect parentR = {
        currR.ox - col * pitchX,
        currR.oy + currR.h - currR.h * k + row * pitchY,
        currR.w * k,
        currR.h * k
      };

      if (parentR.contains(worldX, worldY)) {
        leafIdx = resolveCellAt(worldX, worldY, parentIdx, parentDim, parentR);
        break;
      }

      currIdx = parentIdx;
      currR = parentR;
    }
  }

  // Redirect RESERVED cells to their anchor
  if (leafIdx >= 0 && _model.node(leafIdx).content.type == CellType::RESERVED) {
    return _model.node(leafIdx).content.data.reserved.anchorIndex;
  }

  return leafIdx;
}

int GameView::resolveCellAt(float worldX, float worldY, int nodeIndex, int gridDim,
                            Rect r) const {
  float childCellSize = r.w / (gridDim + (gridDim - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = r.ox + half;
  float startY = r.oy + r.w - half;

  int col = (int)((worldX - startX) / pitch + 0.5f);
  int row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= gridDim || col < 0 || col >= gridDim)
    return -1;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > half || std::abs(worldY - centerY) > half)
    return -1;

  int childIdx = _model.node(nodeIndex).content.data.grid.firstChild + row * gridDim + col;
  const Cell &child = _model.node(childIdx);

  if (child.content.type == CellType::GRID) {
    int childDim = child.content.data.grid.gridDimension;
    return resolveCellAt(worldX, worldY, childIdx, childDim,
                         {centerX - half, centerY - half, childCellSize, childCellSize});
  }

  return childIdx;
}

int GameView::resolveCenterCell(float worldX, float worldY) const {
  constexpr float anchorW = _anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};

  if (anchorR.contains(worldX, worldY)) {
    const Cell &anchor = _model.node(_anchorIndex);
    if (anchor.content.type != CellType::GRID) return _anchorIndex;
    return resolveCellAtWithSizeCheck(worldX, worldY, _anchorIndex, _anchorSize, anchorR);
  }

  int parentIdx = _model.node(_anchorIndex).parentId;
  if (parentIdx < 0) return _anchorIndex;

  const Cell &parent = _model.node(parentIdx);
  if (parent.content.type != CellType::GRID) return _anchorIndex;

  int parentDim = parent.content.data.grid.gridDimension;
  int firstChild = parent.content.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int anchorRow = offset / parentDim;
  int anchorCol = offset % parentDim;
  if (anchorRow < 0 || anchorRow >= parentDim || anchorCol < 0 || anchorCol >= parentDim)
    return _anchorIndex;

  float parentContentW = anchorW * (parentDim + (parentDim - 1) * _gapRatio);
  Rect child = childCellLayout(parentIdx, {0, 0, parentContentW, parentContentW}, anchorRow, anchorCol);
  Rect parentR = {-anchorW * 0.5f - child.ox, -anchorW * 0.5f - child.oy, parentContentW, parentContentW};

  if (!parentR.contains(worldX, worldY))
    return parentIdx;

  return resolveCellAtWithSizeCheck(worldX, worldY, parentIdx, parentDim, parentR);
}

int GameView::resolveCellAtWithSizeCheck(float worldX, float worldY, int nodeIndex,
                                          int gridDim, Rect r) const {
  const Cell &cell = _model.node(nodeIndex);
  if (cell.content.type != CellType::GRID) return nodeIndex;

  float childCellSize = r.w / (gridDim + (gridDim - 1) * _gapRatio);

  constexpr float screenWidthNDC = 2.0f;
  if (childCellSize * _zoom < MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT * screenWidthNDC)
    return cell.parentId;
  if (childCellSize * _zoom < MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD * screenWidthNDC)
    return nodeIndex;

  float pitch = childCellSize * (1 + _gapRatio);
  float half = childCellSize * 0.5f;
  float startX = r.ox + half;
  float startY = r.oy + r.w - half;

  int col = (int)((worldX - startX) / pitch + 0.5f);
  int row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) return nodeIndex;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > half || std::abs(worldY - centerY) > half) return nodeIndex;

  int childIdx = cell.content.data.grid.firstChild + row * gridDim + col;
  const Cell &child = _model.node(childIdx);

  if (child.content.type == CellType::GRID) {
    return resolveCellAtWithSizeCheck(worldX, worldY, childIdx,
                                       child.content.data.grid.gridDimension,
                                       {centerX - half, centerY - half, childCellSize, childCellSize});
  }
  return childIdx;
}

Rect GameView::cellWorldCenter(int targetIdx) const {
  constexpr float anchorW = _anchorWidth;
  Rect r = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  int nodeIdx = _anchorIndex;

  while (nodeIdx != targetIdx) {
    const Cell& node = _model.node(nodeIdx);
    if (node.content.type != CellType::GRID) break;

    int gridDim = node.content.data.grid.gridDimension;
    int firstChild = node.content.data.grid.firstChild;

    int directChild = targetIdx;
    while (_model.node(directChild).parentId != nodeIdx) {
      directChild = _model.node(directChild).parentId;
    }

    int offset = directChild - firstChild;
    int row = offset / gridDim;
    int col = offset % gridDim;
    if (row < 0 || row >= gridDim || col < 0 || col >= gridDim) break;

    Rect child = childCellLayout(nodeIdx, r, row, col);

    if (directChild == targetIdx) {
      return child;
    }

    nodeIdx = directChild;
    r = child;
  }

  return r;
}

// Inverse of childCellLayout: given a child cell rect at (row,col) of a parent
// grid with `parentDim` squares, recover the parent's content rect. This is the
// exact inverse renderAnchor() uses when drawing the parent context behind the
// anchor, so climbing "up" reproduces cell placement faithfully.
static Rect upLayout(const Rect &child, int parentDim, float gapRatio,
                     int row, int col) {
  float k = (float)(parentDim + (parentDim - 1) * gapRatio);
  float pitchX = child.w * (1 + gapRatio);
  float pitchY = child.h * (1 + gapRatio);
  return {child.ox - col * pitchX,
          child.oy + child.h - child.h * k + row * pitchY,
          child.w * k,
          child.h * k};
}

// NDC screen-space centre of a cell under the CURRENT anchor, computed identically
// to how cells are laid out. We climb UP from the anchor and UP from the node to
// their common ancestor, then go back DOWN to the node applying the same layout
// transforms — so the result always matches where the renderer draws that cell,
// under any zoom / focus / pan (fixes anchor-change misplacement). Bounded: climbs
// at most the tree depth, each hop validated.
bool GameView::cellCenterNDC(int nodeIndex, float &nx, float &ny,
                             float &halfNDC) const {
  if (nodeIndex < 0 || nodeIndex >= _model.totalNodes()) return false;
  // Anchor's on-screen content rect (what renderTo passes to renderAnchor).
  Rect R = {-_anchorWidth * 0.5f * _zoom + _panX,
            -_anchorWidth * 0.5f * _aspect * _zoom + _panY,
            _anchorWidth * _zoom,
            _anchorWidth * _aspect * _zoom};

  auto depthOf = [&](int idx) {
    int d = 0;
    while (_model.node(idx).parentId >= 0) { idx = _model.node(idx).parentId; d++; }
    return d;
  };
  // A hop: {parentIdx, row, col} describing where `child` sits in its parent grid.
  auto hopInParent = [&](int child, int &parent, int &row, int &col) {
    const int p = _model.node(child).parentId;
    if (p < 0) return false;
    const Cell &pc = _model.node(p);
    if (pc.content.type != CellType::GRID) return false;
    int dim = pc.content.data.grid.gridDimension;
    int first = pc.content.data.grid.firstChild;
    int off = child - first;
    if (off < 0 || off >= dim * dim) return false;
    parent = p; row = off / dim; col = off % dim;
    return true;
  };

  int a = _anchorIndex, b = nodeIndex;
  int da = depthOf(a), db = depthOf(b);
  struct Hop { int parent, row, col; };
  Hop A[128], B[128]; int nA = 0, nB = 0; // fixed-size (allocation-free)
  int p, r, c;
  while (da > db) { if (!hopInParent(a, p, r, c)) return false; if (nA >= 128) return false; A[nA++] = {p, r, c}; a = p; da--; }
  while (db > da) { if (!hopInParent(b, p, r, c)) return false; if (nB >= 128) return false; B[nB++] = {p, r, c}; b = p; db--; }
  while (a != b) {
    if (nA >= 128 || nB >= 128) return false;
    if (!hopInParent(a, p, r, c)) return false; A[nA++] = {p, r, c}; a = p;
    if (!hopInParent(b, p, r, c)) return false; B[nB++] = {p, r, c}; b = p;
  }
  // a==b is the common ancestor. Climb UP from the anchor's rect via A.
  Rect anc = R;
  for (int i = 0; i < nA; i++) {
    int pd = _model.node(A[i].parent).content.data.grid.gridDimension;
    anc = upLayout(anc, pd, _gapRatio, A[i].row, A[i].col);
  }
  // Walk DOWN the reversed B path (node side) to the target cell rect.
  Rect rc = anc;
  for (int i = nB - 1; i >= 0; i--)
    rc = childCellLayout(B[i].parent, rc, B[i].row, B[i].col);
  nx = rc.cx(); ny = rc.cy();
  halfNDC = rc.halfW();
  return true;
}

// The visual `variation` of the grid that contains this cell (for picking the
// rounded-tile colour family).
int GameView::variationOf(int nodeIndex) const {
  if (nodeIndex < 0 || nodeIndex >= _model.totalNodes()) return 0;
  const int p = _model.node(nodeIndex).parentId;
  if (p >= 0 && _model.node(p).content.type == CellType::GRID)
    return _model.node(p).content.data.grid.variation;
  return 0;
}

void GameView::focusTransform(int targetIdx) {
  Rect r = cellWorldCenter(targetIdx);

  constexpr float maxFocusRatio = 100.0f;
  if (r.w * _zoom > maxFocusRatio * 2.0f)
    return;

  const Cell &cell = _model.node(targetIdx);
  _anchorIndex = targetIdx;
  _anchorSize =
      (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    _anchorDepth++;

  _panX = r.cx() * _zoom + _panX;
  _panY = r.cy() * _aspect * _zoom + _panY;
  _zoom = _zoom * r.w / _anchorWidth;
  if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
}

void GameView::resetView() {
  _anchorIndex = 0;
  const Cell &cell = _model.node(0);
  _anchorSize =
      (cell.content.type == CellType::GRID) ? cell.content.data.grid.gridDimension : GameModel::GRID;
  _anchorDepth = 0;
  _zoom = _fitZoom;   // fit whole board to the shorter window side
  _panX = 0.0f;
  _panY = 0.0f;
}

void GameView::focusCenterCell(int winW, int winH) {
  float worldX, worldY;
  screenToWorld(winW / 2, winH / 2, winW, winH, worldX, worldY);
  int idx = resolveCenterCell(worldX, worldY);
  if (idx < 0 || idx == _anchorIndex) return;

  if (isDescendant(_anchorIndex, idx)) {
    focusTransform(idx);
    return;
  }

  while (!isDescendant(_anchorIndex, idx)) {
    if (!unfocusOneLevel()) return;
  }
  focusTransform(idx);
}

bool GameView::unfocusOneLevel() {
  int parentIdx = _model.node(_anchorIndex).parentId;
  if (parentIdx < 0) return false;

  const Cell &parent = _model.node(parentIdx);
  if (parent.content.type != CellType::GRID) return false;

  int gridDim = parent.content.data.grid.gridDimension;
  int firstChild = parent.content.data.grid.firstChild;
  int offset = _anchorIndex - firstChild;
  int row = offset / gridDim;
  int col = offset % gridDim;

  constexpr float anchorW = _anchorWidth;
  Rect anchorR = {-anchorW * 0.5f, -anchorW * 0.5f, anchorW, anchorW};
  Rect child = childCellLayout(parentIdx, anchorR, row, col);

  float centerX = child.cx();
  float centerY = child.cy();
  float savedZoom = _zoom;

  _anchorIndex = parentIdx;
  _anchorSize = gridDim;
  _anchorDepth = 0;
  for (int i = _anchorIndex; _model.node(i).parentId >= 0; i = _model.node(i).parentId)
    _anchorDepth++;
  _zoom = savedZoom * _anchorWidth / child.w;
  if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
  _panX -= centerX * (savedZoom * _anchorWidth / child.w);
  _panY -= centerY * _aspect * (savedZoom * _anchorWidth / child.w);
  return true;
}

bool GameView::isDescendant(int ancestor, int node) const {
  while (node >= 0) {
    if (node == ancestor) return true;
    node = _model.node(node).parentId;
  }
  return false;
}

void GameView::renderAnchor(const int anchorIndex, const Rect r, const int depth, const int excludeChild) {
  const Cell &cell = _model.node(anchorIndex);
  bool coversScreen = r.ox <= -1.0f && r.oy <= -1.0f &&
                      r.ox + r.w >= 1.0f && r.oy + r.h >= 1.0f;
  if (!coversScreen && cell.parentId >= 0) {
    const Cell &parent = _model.node(cell.parentId);
    if (parent.content.type == CellType::GRID) {
      int parentDim = parent.content.data.grid.gridDimension;
      int firstChild = parent.content.data.grid.firstChild;
      int offset = anchorIndex - firstChild;
      int row = offset / parentDim;
      int col = offset % parentDim;

      float k = (float)(parentDim + (parentDim - 1) * _gapRatio);
      float pitchX = r.w * (1 + _gapRatio);
      float pitchY = r.h * (1 + _gapRatio);

      Rect parentR = {
        r.ox - col * pitchX,
        r.oy + r.h - r.h * k + row * pitchY,
        r.w * k,
        r.h * k
      };
      renderAnchor(cell.parentId, parentR, depth + 1,
                   excludeChild >= 0 ? excludeChild : anchorIndex);
    }
  }

  if (depth >= MAX_PREVIEW_DEPTH)
    return;
  if (r.outsideClip())
    return;
  constexpr float minClipSize = 0.002f;
  if (r.w < minClipSize || r.h < minClipSize)
    return;

  if (cell.content.type == CellType::GRID)
    renderGrid(anchorIndex, r, depth, excludeChild);
  else
    renderCell(anchorIndex, r, depth);
}

// Wave background: a few thin STROKED sinusoid curves drawn directly under the
// NDC matrix. Smooth Skia paths, fixed on screen; only the clock `t` undulates
// them at per-line speed. Cheap enough to be ~free on FPS.
#if !defined(__EMSCRIPTEN__)
void GameView::drawBackground(int winW, int winH, double t) {
  (void)winW; (void)winH;
  if (!_canvas) return;
  const int XSEG = 96;
  const float twoPi = (float)SK_ScalarPI * 2.0f;
  struct L { uint32_t col; float a, cy, amp, f, sp, w; };
  const L Ls[4] = {
      {0xFF4D6A9E, 0.22f, -0.55f, 0.13f, 1.8f, 0.40f, 0.013f},
      {0xFF5A6BB0, 0.18f, -0.16f, 0.10f, 2.6f, 0.56f, 0.010f},
      {0xFF7A5FB0, 0.18f,  0.22f, 0.11f, 2.0f, 0.70f, 0.010f},
      {0xFF3F8A8D, 0.22f,  0.58f, 0.14f, 1.5f, 0.46f, 0.013f},
  };
  for (const L &l : Ls) {
    const float ph = (float)t * l.sp;
    SkPathBuilder b;
    b.moveTo(-1.0f, l.cy + l.amp * std::sin(-l.f * twoPi + ph));
    for (int i = 1; i <= XSEG; i++) {
      const float x = -1.0f + 2.0f * (float)i / (float)XSEG;
      b.lineTo(x, l.cy + l.amp * std::sin(x * l.f * twoPi + ph));
    }
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(l.w);
    p.setColor(SkColorSetA(l.col, (uint8_t)std::lround(l.a * 255.0f)));
    _canvas->drawPath(b.detach(), p);
  }
}
#endif

void GameView::renderTo(SkCanvas *canvas, int winW, int winH) {
  if (!canvas) return;
  _canvas = canvas;
  _targetW = winW;
  _targetH = winH;
  _aspect = (float)winW / (float)winH;
  // Preserve the view when the window resizes: keep the central min-square
  // (side = min(W,H)) mapped to the same world target, and keep that target at
  // screen centre. We only compensate the zoom for the aspect-ratio change (and
  // re-project pan so the world point under the centre stays there) — never jump
  // back to board origin. Explicit reset (R) still re-fits + centres.
  if (winW != _lastWinW || winH != _lastWinH) {
    int ow = _lastWinW, oh = _lastWinH;
    _lastWinW = winW;
    _lastWinH = winH;
    const float newMin = (float)std::min(winW, winH);
    const float newFit = newMin / (float)winW;
    if (ow == 0 || oh == 0) {
      _zoom = newFit;           // first render: fit whole board, centred
      _panX = 0.0f;
      _panY = 0.0f;
    } else {
      const float oldMin = (float)std::min(ow, oh);
      const float zr = (newMin / (float)winW) / (oldMin / (float)ow);
      _zoom *= zr;
      _panX *= zr;
      _panY *= (newMin / (float)winH) / (oldMin / (float)oh);
      if (_zoom < MIN_ZOOM) _zoom = MIN_ZOOM;
      if (_zoom > MAX_ZOOM) _zoom = MAX_ZOOM;
    }
    _fitZoom = newFit;
  }
  _lastVertexCount = 0;

  // NDC -> pixel transform: px = (nx+1)*w/2, py = (1-ny)*h/2
  SkMatrix ndc;
  ndc.setScaleTranslate((SkScalar)(winW * 0.5), (SkScalar)(-winH * 0.5),
                        (SkScalar)(winW * 0.5), (SkScalar)(winH * 0.5));
  canvas->setMatrix(SkM44(ndc));

  uint64_t t0 = nowMs();

  // Fully replace the frame with the background, then alpha-blend the
  // cells/icons/(drag glow) on top so gradients + glow composite correctly.
  canvas->clear(SkColorSetARGB(0xFF, 0x1A, 0x1A, 0x26));
#if !defined(__EMSCRIPTEN__)
  drawBackground(winW, winH, animSeconds());
#endif

  constexpr float anchorW = _anchorWidth;
  float ox = -anchorW * 0.5f * _zoom + _panX;
  float oy = -anchorW * 0.5f * _aspect * _zoom + _panY;
  float cw = anchorW * _zoom;
  float ch = anchorW * _aspect * _zoom;

  renderAnchor(_anchorIndex, {ox, oy, cw, ch}, 0);

  if (_model.hasDrag()) {
    int dragId = _model.dragItemId();
    int dragAmount = _model.dragAmount();

    if (!_dragWasActive) {
      _dragAnimStartMs = nowMs();
    }

    double elapsed = (double)(nowMs() - _dragAnimStartMs) / 1000.0;
    double t = elapsed / _dragAnimDuration;
    if (t >= 1.0) {
      t = 1.0;
    }
    float eased = (float)(t * (2.0 - t));

    float scale = (0.5f + 0.5f * eased) / _zoom;

    float sx = _dragWX * _zoom + _panX;
    float sy = _dragWY * _aspect * _zoom + _panY;
    const float *elemCol = (dragId >= 0 && dragId < GameModel::ELEMS) ? _elemColors[dragId] : _white;
    int dragIcon = dragId;
    renderCellItems(sx, sy, dragAmount, elemCol, scale, dragIcon);
  }
  _dragWasActive = _model.hasDrag();

  // Draw the particle/smoke layer on top of the scene. Each source's cell node is
  // resolved to its on-screen NDC position through the SAME anchor-tree walk the
  // renderer uses, so smoke always sits exactly on its cell (any zoom/focus/pan).
  if (_particles)
    _particles->draw(_canvas, winW, winH,
                     [this](int n, float &x, float &y, float &h) {
                       return cellCenterNDC(n, x, y, h);
                     });

  uint64_t t1 = nowMs();
  _lastGenMs = (float)(t1 - t0);
  _lastDrawMs = (float)(nowMs() - t1);

  _canvas = nullptr;
}

bool GameView::screenToGrid(int pixelX, int pixelY, int winW, int winH, int &row,
                            int &col) const {
  float worldX, worldY;
  screenToWorld(pixelX, pixelY, winW, winH, worldX, worldY);

  int size = _anchorSize;

  float childCellSize = _anchorWidth / (size + (size - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = size * pitch;
  float halfContent = childCellSize * 0.5f;

  float startX = -totalSize * 0.5f + pitch * 0.5f;
  float startY = totalSize * 0.5f - pitch * 0.5f;

  col = (int)((worldX - startX) / pitch + 0.5f);
  row = (int)((startY - worldY) / pitch + 0.5f);

  if (row < 0 || row >= size || col < 0 || col >= size)
    return false;

  float centerX = startX + col * pitch;
  float centerY = startY - row * pitch;
  if (std::abs(worldX - centerX) > halfContent ||
      std::abs(worldY - centerY) > halfContent)
    return false;

  return true;
}

void GameView::screenToWorld(int pixelX, int pixelY, int winW, int winH, float &worldX,
                             float &worldY) const {
  float aspect = (float)winW / (float)winH;
  worldX = ((2.0f * pixelX / (float)winW) - 1.0f - _panX) / _zoom;
  worldY = ((1.0f - (2.0f * pixelY / (float)winH)) - _panY) / aspect / _zoom;
}

float GameView::gridToWorldX(int col) const {
  float childCellSize = _anchorWidth / (_anchorSize + (_anchorSize - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = _anchorSize * pitch;
  return -totalSize * 0.5f + pitch * 0.5f + col * pitch;
}

float GameView::gridToWorldY(int row) const {
  float childCellSize = _anchorWidth / (_anchorSize + (_anchorSize - 1) * _gapRatio);
  float pitch = childCellSize * (1 + _gapRatio);
  float totalSize = _anchorSize * pitch;
  return totalSize * 0.5f - pitch * 0.5f - row * pitch;
}

void GameView::zoom(float factor, float mouseNX, float mouseNY) {
  float oldZoom = _zoom;
  _zoom *= factor;
  if (_zoom < MIN_ZOOM)
    _zoom = MIN_ZOOM;
  if (_zoom > MAX_ZOOM)
    _zoom = MAX_ZOOM;
  float actualFactor = _zoom / oldZoom;
  _panX = mouseNX * (1.0f - actualFactor) + _panX * actualFactor;
  _panY = mouseNY * (1.0f - actualFactor) + _panY * actualFactor;
}

void GameView::startPan(int pixelX, int pixelY) {
  _isPanning = true;
  _lastPanX = pixelX;
  _lastPanY = pixelY;
}

void GameView::continuePan(int pixelX, int pixelY, int winW, int winH) {
  float deltaX = 2.0f * (pixelX - _lastPanX) / (float)winW;
  float deltaY = -2.0f * (pixelY - _lastPanY) / (float)winH;
  _panX += deltaX;
  _panY += deltaY;
  _lastPanX = pixelX;
  _lastPanY = pixelY;
}

void GameView::endPan() { _isPanning = false; }

std::size_t GameView::allocatedBytes() const {
  std::size_t bytes = 0;
  bytes += _icons.size() * sizeof(IconStyle);  // cached icon paths + paints
  return bytes;
}