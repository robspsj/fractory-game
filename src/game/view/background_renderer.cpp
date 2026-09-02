#include "background_renderer.hpp"
#include "game_view.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPathBuilder.h"
#if !defined(__EMSCRIPTEN__)
void BackgroundRenderer::drawBackground(GameView &view, int winW, int winH, double t) {
  (void)winW; (void)winH;
  if (!view._canvas) return;
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
    view._canvas->drawPath(b.detach(), p);
  }
}
#endif
