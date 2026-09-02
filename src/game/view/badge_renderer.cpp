#include "badge_renderer.hpp"
#include "game_view.hpp"
#include "../../font.hpp"
#include <algorithm>
#include <cstdio>
#include "include/core/SkCanvas.h"

void BadgeRenderer::drawCountBadge(GameView &view, const Rect &r, int count) {
  if (!view._canvas) return;
  const float pw = r.w * (float)view._targetW * 0.5f;
  const float ph = r.h * (float)view._targetH * 0.5f;
  const float cellMin = std::min(pw, ph);
  if (cellMin < 18.0f) return;
  char buf[16]; snprintf(buf, sizeof(buf), "X%d", count);
  const float scale = std::clamp(cellMin / 48.0f, 1.0f, 6.0f);
  const float textW = textWidth(buf, scale, &view._bench);
  const float textH = textHeight(scale, &view._bench);
  const float cellLeft = (r.ox + 1.0f) * 0.5f * (float)view._targetW;
  const float cellRight = (r.ox + r.w + 1.0f) * 0.5f * (float)view._targetW;
  const float cellBottom = (1.0f - r.oy) * 0.5f * (float)view._targetH;
  const float cellTop = (1.0f - (r.oy + r.h)) * 0.5f * (float)view._targetH;
  const float padX = 3.0f, padY = 2.0f;
  const float pillW = textW + padX * 2.0f;
  const float pillH = textH + padY * 2.0f;
  float pillX = cellRight - pillW;
  float pillY = cellBottom - pillH;
  if (pillX < cellLeft) pillX = cellLeft;
  if (pillY < cellTop) pillY = cellTop;
  view._canvas->save();
  view._canvas->resetMatrix();
  drawText(view._canvas, pillX + padX, pillY + padY, buf, scale, &view._bench);
  view._canvas->restore();
}
