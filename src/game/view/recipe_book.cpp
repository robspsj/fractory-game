#include "recipe_book.hpp"
#include "game_view.hpp"
#include "icon_renderer.hpp"
#include "../model/game_model.hpp"
#include "../heat_constants.hpp"
#include "../../font.hpp"
#include <algorithm>
#include <cstdio>
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

namespace {

constexpr float kMargin = 22.f;
constexpr float kSidebarW = 196.f;
constexpr float kSidebarRowH = 30.f;
constexpr float kCell = 30.f;      // mini-grid cell size (px)
constexpr float kIconHalf = 13.f;  // item icon half-extent (px)
constexpr float kTextScale = 1.45f;
constexpr float kSmallScale = 1.15f;

const char *kCatName[(int)BookCategory::COUNT] = {"Recipes", "Machines", "Reactions",
                                                  "Stacking", "Quantum"};

struct Layout {
  float x0, y0, w, h;                       // panel
  float sideX, sideY;                       // sidebar rows origin
  float contentX, contentY, contentW, contentH;
};

Layout layoutFor(int winW, int winH) {
  Layout L;
  L.x0 = kMargin;
  L.y0 = kMargin;
  L.w = (float)winW - 2.f * kMargin;
  L.h = (float)winH - 2.f * kMargin;
  L.sideX = L.x0 + 12.f;
  L.sideY = L.y0 + 52.f;
  L.contentX = L.sideX + kSidebarW + 22.f;
  L.contentY = L.y0 + 52.f;
  L.contentW = L.x0 + L.w - 14.f - L.contentX;
  L.contentH = L.y0 + L.h - 14.f - L.contentY;
  return L;
}

void fillRect(SkCanvas *c, float x, float y, float w, float h, SkColor col, float r = 4.f) {
  if (w <= 0.f || h <= 0.f) return;
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor(col);
  c->drawRoundRect(SkRect::MakeXYWH(x, y, w, h), r, r, p);
}

void strokeRect(SkCanvas *c, float x, float y, float w, float h, SkColor col, float r = 4.f) {
  if (w <= 0.f || h <= 0.f) return;
  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.f);
  p.setColor(col);
  c->drawRoundRect(SkRect::MakeXYWH(x, y, w, h), r, r, p);
}

SkColor withAlpha(SkColor c, float a) {
  return SkColorSetARGB((uint8_t)(255.f * std::clamp(a, 0.f, 1.f)), SkColorGetR(c),
                        SkColorGetG(c), SkColorGetB(c));
}

SkColor lerpColor(SkColor a, SkColor b, float t) {
  t = std::clamp(t, 0.f, 1.f);
  auto ch = [&](int shift) {
    return (uint8_t)(SkColorGetR(a) * (1 - t) + SkColorGetR(b) * t);
  };
  (void)ch;
  return SkColorSetARGB(255,
                        (uint8_t)(SkColorGetR(a) * (1 - t) + SkColorGetR(b) * t),
                        (uint8_t)(SkColorGetG(a) * (1 - t) + SkColorGetG(b) * t),
                        (uint8_t)(SkColorGetB(a) * (1 - t) + SkColorGetB(b) * t));
}

// Item icon through the normal icon renderer, so the book shows the very same
// vector art the board does.
void iconAt(GameView &view, SkCanvas *canvas, float cx, float cy, float halfPx, int itemId) {
  view.drawItemIcon(canvas, cx, cy, halfPx, itemId);
}

void doneMark(SkCanvas *c, float x, float y, bool done) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor(done ? SkColorSetARGB(255, 92, 208, 120) : SkColorSetARGB(110, 118, 126, 140));
  c->drawCircle(x, y, 4.5f, p);
}

// Row background: performed entries get a green wash so progress is visible at
// a glance while every entry stays listed.
SkColor rowColor(bool done) {
  return done ? SkColorSetARGB(165, 28, 58, 38) : SkColorSetARGB(150, 30, 34, 44);
}

void rowStatus(SkCanvas *c, float xRight, float y, bool done) {
  if (!done) return;   // only mark what is finished; unseen rows stay quiet
  const char *t = "done";
  drawText(c, xRight - textWidth(t, kSmallScale), y, t, kSmallScale);
}

int catTotal(const InteractConfig &ic, BookCategory c) {
  switch (c) {
    case BookCategory::Recipes:   return (int)ic.crafting.size();
    case BookCategory::Machines:  return (int)ic.stationRecipes.size();
    case BookCategory::Reactions: return (int)ic.reactions.size();
    case BookCategory::Stacking:  return (int)BookPhysics::Split + 1;
    case BookCategory::Quantum:   return 1;
    default:                      return 0;
  }
}

int catDone(const BookState &bs, BookCategory c) {
  const std::vector<uint8_t> *v = nullptr;
  if (c == BookCategory::Recipes) v = &bs.recipes;
  else if (c == BookCategory::Machines) v = &bs.machines;
  else if (c == BookCategory::Reactions) v = &bs.reactions;
  if (v) {
    int n = 0;
    for (uint8_t f : *v) n += f ? 1 : 0;
    return n;
  }
  if (c == BookCategory::Stacking) {
    int n = 0;
    for (int i = 0; i <= (int)BookPhysics::Split; i++) n += bs.has(c, i) ? 1 : 0;
    return n;
  }
  return bs.has(c, (int)BookPhysics::Quantum) ? 1 : 0;
}

}  // namespace

void RecipeBook::setCategory(int c) {
  if (c < 0 || c >= (int)BookCategory::COUNT) return;
  if (c == _cat) return;
  _cat = c;
  _scroll = 0.f;
}

void RecipeBook::cycle(int dir) {
  const int n = (int)BookCategory::COUNT;
  int c = ((_cat + dir) % n + n) % n;
  setCategory(c);
}

void RecipeBook::scrollBy(float dy) {
  _scroll = std::max(0.f, std::min(_scroll - dy * 34.f, std::max(0.f, _contentH - _viewH)));
}

bool RecipeBook::click(int px, int py, int winW, int winH) {
  if (!_open) return false;
  const Layout L = layoutFor(winW, winH);
  const float fx = (float)px, fy = (float)py;
  // Sidebar tabs.
  for (int i = 0; i < (int)BookCategory::COUNT; i++) {
    const float ry = L.sideY + i * kSidebarRowH;
    if (fx >= L.sideX && fx <= L.sideX + kSidebarW && fy >= ry && fy < ry + kSidebarRowH - 4.f) {
      setCategory(i);
      return true;
    }
  }
  // Everything else inside the panel is swallowed (the book is modal).
  return fx >= L.x0 && fx <= L.x0 + L.w && fy >= L.y0 && fy <= L.y0 + L.h;
}

void RecipeBook::draw(GameView &view, SkCanvas *canvas, int winW, int winH) {
  if (!_open || !canvas) return;
  const GameModel &model = view._model;
  const InteractConfig &ic = model.interactConfig();
  const BookState &bs = model.book();
  const Layout L = layoutFor(winW, winH);

  canvas->resetMatrix();
  fillRect(canvas, L.x0, L.y0, L.w, L.h, SkColorSetARGB(252, 14, 16, 22), 10.f);
  strokeRect(canvas, L.x0, L.y0, L.w, L.h, SkColorSetARGB(200, 104, 116, 142), 10.f);
  drawText(canvas, L.x0 + 16.f, L.y0 + 14.f, "RECIPE BOOK", 2.0f);
  drawText(canvas, L.x0 + 168.f, L.y0 + 20.f, "everything the world can do  -  green row = done",
           kSmallScale);
  drawText(canvas, L.x0 + L.w - 224.f, L.y0 + 20.f, "F2 close   1-5 tabs   arrows scroll", kSmallScale);

  // ── Sidebar ──────────────────────────────────────────────────────────────
  for (int i = 0; i < (int)BookCategory::COUNT; i++) {
    const BookCategory cat = (BookCategory)i;
    const float ry = L.sideY + i * kSidebarRowH;
    const bool active = (i == _cat);
    if (active) {
      fillRect(canvas, L.sideX - 6.f, ry - 2.f, kSidebarW, kSidebarRowH - 4.f,
               SkColorSetARGB(230, 52, 62, 84), 6.f);
      fillRect(canvas, L.sideX - 6.f, ry - 2.f, 4.f, kSidebarRowH - 4.f,
               SkColorSetARGB(255, 132, 176, 255), 2.f);
    }
    char label[64];
    snprintf(label, sizeof label, "%d  %s", i + 1, kCatName[i]);
    drawText(canvas, L.sideX + 6.f, ry + 6.f, label, kTextScale);
    const int total = catTotal(ic, cat);
    snprintf(label, sizeof label, "%d/%d", catDone(bs, cat), total);
    drawText(canvas, L.sideX + kSidebarW - textWidth(label, kSmallScale) - 10.f, ry + 9.f, label,
             kSmallScale);
  }
  strokeRect(canvas, L.sideX + kSidebarW + 6.f, L.sideY - 4.f, 1.f, L.contentH + 8.f,
             SkColorSetARGB(90, 110, 120, 140), 0.f);

  // ── Content ──────────────────────────────────────────────────────────────
  canvas->save();
  canvas->clipRect(SkRect::MakeXYWH(L.contentX, L.contentY, L.contentW, L.contentH));
  const float top = L.contentY;
  float y = top - _scroll;
  const float cx = L.contentX;
  const float cw = L.contentW;
  const SkColor cellBg = SkColorSetARGB(190, 46, 52, 66);
  char buf[128];

  switch ((BookCategory)_cat) {
    case BookCategory::Recipes: {
      drawText(canvas, cx, y, "Hold the hammer (item 3) and right-click a cell of the shape",
               kSmallScale);
      y += 26.f;
      for (int i = 0; i < (int)ic.crafting.size(); i++) {
        const CraftingRule &r = ic.crafting[i];
        const int rows = r.maxDr - r.minDr + 1, cols = r.maxDc - r.minDc + 1;
        // Rows are at least 2 grid rows tall so the list reads as an even stack
        // regardless of how tall each shape is.
        const float eh = std::max(2, rows) * kCell + 18.f;
        const bool done = bs.has(BookCategory::Recipes, i);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f, y + eh * 0.5f - 3.f, done);
          const float gx = cx + 26.f, gy = y + 8.f;
          for (int rr = 0; rr < rows; rr++) {
            for (int cc = 0; cc < cols; cc++) {
              const float px = gx + cc * kCell, py = gy + rr * kCell;
              int itemId = -1;
              for (const CraftEntry &e : r.entries)
                if (e.dr - r.minDr == rr && e.dc - r.minDc == cc) { itemId = e.itemId; break; }
              if (itemId < 0) {
                // Empty slot: visible on purpose — where a cell is empty is part
                // of the shape (an L and a 2x2 block differ by exactly this).
                fillRect(canvas, px + 1.f, py + 1.f, kCell - 2.f, kCell - 2.f,
                         SkColorSetARGB(120, 26, 30, 40), 4.f);
                strokeRect(canvas, px + 1.f, py + 1.f, kCell - 2.f, kCell - 2.f,
                           SkColorSetARGB(130, 108, 118, 138), 4.f);
                continue;
              }
              fillRect(canvas, px, py, kCell, kCell, cellBg, 5.f);
              iconAt(view, canvas, px + kCell * 0.5f, py + kCell * 0.5f, kIconHalf, itemId);
            }
          }
          const float ax = gx + cols * kCell + 14.f;
          drawText(canvas, ax, y + eh * 0.5f - 10.f, "->", kTextScale);
          const float rx = ax + 32.f;
          const float ry2 = y + 8.f;
          fillRect(canvas, rx, ry2, kCell, kCell, cellBg, 5.f);
          iconAt(view, canvas, rx + kCell * 0.5f, ry2 + kCell * 0.5f, kIconHalf, r.result);
          snprintf(buf, sizeof buf, "%s x%d", view.itemName(r.result).c_str(), r.resultCount);
          drawText(canvas, rx + kCell + 10.f, ry2 + 4.f, buf, kTextScale);
          snprintf(buf, sizeof buf, "recipe %d", i);
          drawText(canvas, rx + kCell + 10.f, ry2 + 22.f, buf, kSmallScale);
          rowStatus(canvas, cx + cw - 14.f, y + eh * 0.5f - 10.f, done);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Machines: {
      drawText(canvas, cx, y, "Fill the inputs; output appears after the duration", kSmallScale);
      y += 26.f;
      for (int i = 0; i < (int)ic.stationRecipes.size(); i++) {
        const StationRecipe &r = ic.stationRecipes[i];
        const float eh = std::max(1, r.sizeR) * kCell + 18.f;
        const bool done = bs.has(BookCategory::Machines, i);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f, y + eh * 0.5f - 3.f, done);
          const float gx = cx + 26.f, gy = y + 8.f;
          for (int rr = 0; rr < r.sizeR; rr++) {
            for (int cc = 0; cc < r.sizeC; cc++) {
              const int li = rr * r.sizeC + cc;
              ReserveRole role = (li < (int)r.layout.size()) ? r.layout[li] : ReserveRole::NONE;
              SkColor col = SkColorSetARGB(190, 70, 76, 92);
              if (rr == 0 && cc == 0)
                col = SkColorSetARGB(220, 150, 96, 220);  // anchor = the machine cell
              else if (role == ReserveRole::INPUT) col = SkColorSetARGB(210, 70, 150, 90);
              else if (role == ReserveRole::OUTPUT) col = SkColorSetARGB(210, 200, 120, 50);
              else if (role == ReserveRole::BUFFER) col = SkColorSetARGB(210, 70, 120, 180);
              fillRect(canvas, gx + cc * kCell, gy + rr * kCell, kCell - 2.f, kCell - 2.f, col, 4.f);
            }
          }
          const float tx = gx + r.sizeC * kCell + 14.f;
          const char *na = r.inputA >= 0 ? view.itemName(r.inputA).c_str() : "-";
          const char *nb = r.inputB >= 0 ? view.itemName(r.inputB).c_str() : "-";
          snprintf(buf, sizeof buf, "%s + %s  ->  %s x%d", na, nb,
                   view.itemName(r.output).c_str(), r.outputCount);
          drawText(canvas, tx, y + 8.f, buf, kTextScale);
          snprintf(buf, sizeof buf, "%d ticks   (machine %d, %dx%d)", r.duration, r.id, r.sizeR,
                   r.sizeC);
          drawText(canvas, tx, y + 30.f, buf, kSmallScale);
          drawText(canvas, tx, y + 48.f, "violet = machine   green = input", kSmallScale);
          drawText(canvas, tx, y + 66.f, "orange = output   blue = buffer", kSmallScale);
          rowStatus(canvas, cx + cw - 14.f, y + 8.f, done);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Reactions: {
      drawText(canvas, cx, y, "Item reactions - shift-click one item next to another",
               kSmallScale);
      y += 26.f;
      for (int i = 0; i < (int)ic.reactions.size(); i++) {
        const ReactionRule &r = ic.reactions[i];
        const float eh = kCell + 18.f;
        const bool done = bs.has(BookCategory::Reactions, i);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f, y + eh * 0.5f - 3.f, done);
          float px = cx + 26.f;
          const float py = y + 8.f;
          const float nameY = py + kCell + 4.f;
          fillRect(canvas, px, py, kCell, kCell, cellBg, 5.f);
          iconAt(view, canvas, px + kCell * 0.5f, py + kCell * 0.5f, kIconHalf, r.itemA);
          drawText(canvas, px, nameY, view.itemName(r.itemA).c_str(), kSmallScale);
          px += kCell + 10.f;
          drawText(canvas, px, py + 6.f, "+", kTextScale);
          px += 16.f;
          fillRect(canvas, px, py, kCell, kCell, cellBg, 5.f);
          iconAt(view, canvas, px + kCell * 0.5f, py + kCell * 0.5f, kIconHalf, r.itemB);
          drawText(canvas, px, nameY, view.itemName(r.itemB).c_str(), kSmallScale);
          px += kCell + 10.f;
          drawText(canvas, px, py + 6.f, "->", kTextScale);
          px += 32.f;
          fillRect(canvas, px, py, kCell, kCell, cellBg, 5.f);
          iconAt(view, canvas, px + kCell * 0.5f, py + kCell * 0.5f, kIconHalf, r.result);
          snprintf(buf, sizeof buf, "%s x%d", view.itemName(r.result).c_str(), r.resultCount);
          drawText(canvas, px + kCell + 10.f, py + 6.f, buf, kTextScale);
          drawText(canvas, px, nameY, "result", kSmallScale);
          rowStatus(canvas, cx + cw - 14.f, py + 6.f, done);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Stacking: {
      drawText(canvas, cx, y, "How piles behave on their own (tick-driven)", kSmallScale);
      y += 26.f;
      struct Entry { BookPhysics id; const char *title; const char *desc; const char *desc2; };
      char split1[80], split2[80];
      snprintf(split1, sizeof split1,
               "Above %d units a stack may split in half to an empty neighbour.",
               Heat::ITEM_SPILL_COMFORT);
      snprintf(split2, sizeof split2, "Chance %d%% + %d%% per extra unit, capped at %d%%.",
               Heat::ITEM_SPILL_CHANCE_BASE, Heat::ITEM_SPILL_CHANCE_PER,
               Heat::ITEM_SPILL_CHANCE_MAX);
      const Entry entries[] = {
          {BookPhysics::Grow, "Grow", "An item acts and its stack gains one unit (no cap).", ""},
          {BookPhysics::Shrink, "Shrink", "An item acts and its stack loses one unit.", ""},
          {BookPhysics::Destroy, "Vanish", "A stack of one loses its unit and the cell empties.", ""},
          {BookPhysics::Merge, "Merge", "Drag a stack onto the same item: the counts add up.", ""},
          {BookPhysics::Split, "Split", split1, split2},
      };
      for (const Entry &e : entries) {
        const float eh = e.desc2[0] ? 58.f : 40.f;
        const bool done = bs.has(BookCategory::Stacking, (int)e.id);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f, y + 14.f, done);
          drawText(canvas, cx + 26.f, y + 6.f, e.title, kTextScale);
          drawText(canvas, cx + 26.f, y + 24.f, e.desc, kSmallScale);
          if (e.desc2[0]) drawText(canvas, cx + 26.f, y + 40.f, e.desc2, kSmallScale);
          rowStatus(canvas, cx + cw - 14.f, y + 6.f, done);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Quantum: {
      drawText(canvas, cx, y, "Heat - the only way matter appears from nothing", kSmallScale);
      y += 26.f;
      const float eh = 172.f;
      const bool qDone = bs.has(BookCategory::Quantum, (int)BookPhysics::Quantum);
      if (y + eh > top && y < top + L.contentH) {
        fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(qDone));
        doneMark(canvas, cx + 12.f, y + 14.f, qDone);
        drawText(canvas, cx + 26.f, y + 6.f, "Quantum fluctuation", kTextScale);
        rowStatus(canvas, cx + cw - 14.f, y + 6.f, qDone);
        // Heat bar 0..255 on its own row, threshold marked, then short text
        // lines below (drawText clips at 64 chars, so nothing longer is safe).
        const float bw = std::min(cw - 60.f, 700.f);
        const float bx = cx + 26.f, by = y + 40.f, bh = 18.f;
        const int segs = 64;
        for (int s = 0; s < segs; s++) {
          const float t = (float)s / (float)(segs - 1);
          fillRect(canvas, bx + (bw / segs) * s, by, bw / segs + 1.f, bh,
                   lerpColor(SkColorSetARGB(255, 40, 66, 130), SkColorSetARGB(255, 255, 96, 24), t),
                   0.f);
        }
        const float tx = bx + bw * (float)Heat::QUANTUM_MIN_HEAT / 255.f;
        SkPaint tick;
        tick.setAntiAlias(true);
        tick.setColor(SK_ColorWHITE);
        tick.setStrokeWidth(2.f);
        canvas->drawLine(tx, by - 5.f, tx, by + bh + 5.f, tick);
        snprintf(buf, sizeof buf, "fluctuation from %d", Heat::QUANTUM_MIN_HEAT);
        drawText(canvas, tx + 6.f, by - 20.f, buf, kSmallScale);
        drawText(canvas, bx, by + bh + 6.f, "0", kSmallScale);
        drawText(canvas, bx + bw - 22.f, by + bh + 6.f, "255", kSmallScale);
        float ty = by + bh + 26.f;
        snprintf(buf, sizeof buf, "Hold RIGHT mouse on a cell: +%d heat per tick.",
                 Heat::QUANTUM_HOLD_HEAT_PER_TICK);
        drawText(canvas, bx, ty, buf, kSmallScale);
        ty += 18.f;
        snprintf(buf, sizeof buf, "An EMPTY cell at %d+ heat rolls %d%% per heat pass (~1.6 s)",
                 Heat::QUANTUM_MIN_HEAT, Heat::QUANTUM_CHANCE_PCT);
        drawText(canvas, bx, ty, buf, kSmallScale);
        ty += 18.f;
        drawText(canvas, bx, ty, "and births a random item; the birth absorbs the heat.", kSmallScale);
        ty += 18.f;
        drawText(canvas, bx, ty, "Hot cells make items act up to 5x more often.", kSmallScale);
      }
      y += eh;
      break;
    }
    default:
      break;
  }

  const float total = (y + _scroll) - top;
  canvas->restore();
  _contentH = std::max(0.f, total);
  _viewH = L.contentH;
  _scroll = std::max(0.f, std::min(_scroll, std::max(0.f, _contentH - _viewH)));
  if (_contentH > _viewH) {  // slim scroll indicator
    const float trackX = L.contentX + L.contentW + 2.f;
    const float frac = _viewH / _contentH;
    const float barH = std::max(24.f, L.contentH * frac);
    const float barY = L.contentY + (L.contentH - barH) * (_scroll / (_contentH - _viewH));
    fillRect(canvas, trackX, L.contentY, 3.f, L.contentH, SkColorSetARGB(70, 110, 120, 140), 1.f);
    fillRect(canvas, trackX, barY, 3.f, barH, SkColorSetARGB(200, 150, 160, 180), 1.f);
  }
}
