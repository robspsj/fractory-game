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

const char *kCatName[(int)BookCategory::COUNT] = {"Recipes", "Machines", "Reactions",
                                                  "Stacking", "Quantum"};

// ─── Responsive metrics ─────────────────────────────────────────────────────
// Everything is derived from the window size so the book works from a small
// web canvas up to a 4K window. ui scales geometry, text is floored so it stays
// legible on small windows (vertical overflow is handled by the scroll view
// instead of shrinking the type into nothing).
struct Metrics {
  float ui = 1.f;      // geometry scale
  float o = 1.f;       // structural offset scale (gentler than ui)
  float text = 1.45f;  // body text scale
  float small = 1.15f; // secondary text scale
  float cell = 30.f;   // mini-grid cell size (px)
  float icon = 13.f;   // item icon half-extent (px)
  float gap = 18.f;    // line gap (px)
  float margin = 22.f; // panel margin
};

struct Layout {
  Metrics m;
  float x0, y0, w, h;                       // panel
  bool tabsOnTop = false;                   // narrow window: tab strip, not sidebar
  float tabH = 26.f;                        // tab strip height (tabsOnTop)
  float sideX, sideY, sideW, rowH;          // sidebar (or strip) geometry
  float contentX, contentY, contentW, contentH;
  float titleH = 42.f;
};

Metrics metricsFor(int winW, int winH) {
  Metrics m;
  const float minDim = (float)std::min(winW, winH);
  m.ui = std::clamp(minDim / 700.f, 0.55f, 1.5f);
  m.o = std::clamp(m.ui, 0.75f, 1.3f);
  m.text = std::clamp(1.45f * m.ui, 1.05f, 1.7f);
  m.small = std::clamp(1.15f * m.ui, 0.95f, 1.35f);
  m.cell = std::clamp(30.f * m.ui, 18.f, 36.f);
  m.icon = m.cell * 0.44f;
  m.gap = std::clamp(18.f * m.ui, 13.f, 22.f);
  m.margin = std::clamp(22.f * m.ui, 8.f, 30.f);
  return m;
}

Layout layoutFor(int winW, int winH) {
  Layout L;
  L.m = metricsFor(winW, winH);
  const Metrics &m = L.m;
  L.x0 = m.margin;
  L.y0 = m.margin;
  L.w = (float)winW - 2.f * m.margin;
  L.h = (float)winH - 2.f * m.margin;
  L.titleH = 34.f * m.o + 8.f;
  L.sideW = std::clamp(196.f * m.o, 112.f, 232.f);
  L.rowH = std::clamp(30.f * m.o, 20.f, 34.f);
  L.tabH = std::clamp(26.f * m.o, 18.f, 32.f);
  // Tabs move to a top strip when a left column would leave the content too
  // narrow (small windows, phones, very tall/portrait aspect ratios).
  L.tabsOnTop = (L.w < 780.f) || (L.w - L.sideW - 30.f < 400.f);
  if (L.tabsOnTop) {
    L.sideX = L.x0 + 10.f;
    L.sideY = L.y0 + L.titleH;
    L.contentX = L.x0 + 12.f;
    L.contentY = L.sideY + L.tabH + 10.f;
    L.contentW = L.w - 24.f;
    L.contentH = L.y0 + L.h - 14.f - L.contentY;
  } else {
    L.sideX = L.x0 + 12.f;
    L.sideY = L.y0 + L.titleH;
    L.contentX = L.sideX + L.sideW + 20.f;
    L.contentY = L.y0 + L.titleH;
    L.contentW = L.x0 + L.w - 14.f - L.contentX;
    L.contentH = L.y0 + L.h - 14.f - L.contentY;
  }
  L.contentW = std::max(120.f, L.contentW);
  L.contentH = std::max(60.f, L.contentH);
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

SkColor lerpColor(SkColor a, SkColor b, float t) {
  t = std::clamp(t, 0.f, 1.f);
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

void doneMark(SkCanvas *c, float x, float y, float r, bool done) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor(done ? SkColorSetARGB(255, 92, 208, 120) : SkColorSetARGB(110, 118, 126, 140));
  c->drawCircle(x, y, r, p);
}

// Row background: performed entries get a green wash so progress is visible at
// a glance while every entry stays listed.
SkColor rowColor(bool done) {
  return done ? SkColorSetARGB(165, 28, 58, 38) : SkColorSetARGB(150, 30, 34, 44);
}

void rowStatus(SkCanvas *c, float xRight, float y, bool done, float small) {
  if (!done) return;   // only mark what is finished; unseen rows stay quiet
  const char *t = "done";
  drawText(c, xRight - textWidth(t, small), y, t, small);
}

// Is this automatic behaviour switched on? Feature flags live in the model
// (Config::itemGrow/…); manual transformations (merge) are always available.
bool physicsEnabled(const GameModel &m, BookPhysics id) {
  switch (id) {
    case BookPhysics::Grow: return m.itemGrow();
    case BookPhysics::Shrink:
    case BookPhysics::Destroy: return m.itemShrink();
    case BookPhysics::Split: return m.itemSplit();
    default: return true;
  }
}

// ─── Activation help ────────────────────────────────────────────────────────
// "How do I make this happen?" — the book's instructions live in exactly these
// tables, so when a mechanic's input changes (or a flag is added/removed) this
// is the only place to update. Keep them in sync with the code paths:
//   Recipes   -> GameModel::tryCraftWithDrag   (held tool + right-click)
//   Machines  -> StationCell::place / tick     (footprint + inputs)
//   Reactions -> ItemCell::interact            (shift-click)
//   Stacking  -> ItemCell::collectTickPlan     (tick-driven, flag-gated)
//   Quantum   -> Game::step heat pump + GameModel::quantumFluctuation
const char *kCatHow[(int)BookCategory::COUNT] = {
    /* Recipes   */ "How: hold the hammer (item 3), right-click any cell of the shape.",
    /* Machines  */ "How: shift-click an INPUT cell (green) while holding an item to load it.",
    /* Reactions */ "How: shift-click an item that sits next to another item.",
    /* Stacking  */ "How: automatic, gated by a feature flag per row (see the 'off' rows).",
    /* Quantum   */ "",   // single entry: its own how-line carries the instruction
};

// Extra context line per category (tools, sources, caveats). Empty = none.
const char *kCatNote[(int)BookCategory::COUNT] = {
    /* Recipes   */ "The hammer itself: two wood react into one, or craft the L shape below.",
    /* Machines  */ "Machines are scattered by the random world; hand-placing has no input yet.",
    /* Reactions */ "Two identical items work too: wood + wood -> a hammer.",
    /* Stacking  */ "Enable with --item-dynamics (or --item-grow/-shrink/-split individually).",
    /* Quantum   */ "",
};

// Per-entry activation for the code-defined entries (Stacking rows and the
// Quantum mechanic) — these differ row by row, so they get their own line.
const char *kPhysicsHow[(int)BookPhysics::COUNT] = {
    /* Grow    */ "How: automatic while --item-grow / --item-dynamics is on.",
    /* Shrink  */ "How: automatic while --item-shrink / --item-dynamics is on.",
    /* Destroy */ "How: automatic while --item-shrink / --item-dynamics is on.",
    /* Merge   */ "How: drag a stack onto a cell holding the same item.",
    /* Split   */ "How: automatic while --item-split / --item-dynamics is on.",
    /* Quantum */ "How: hold RIGHT mouse on an EMPTY cell until it passes the threshold.",
};

// Draw the per-category activation help (and its context note) under the
// section intro; returns the y advance consumed. Empty strings are skipped.
float drawCatHelp(SkCanvas *c, float x, float y, BookCategory cat, const Metrics &m) {
  float adv = 0.f;
  const char *how = kCatHow[(int)cat];
  const char *note = kCatNote[(int)cat];
  if (how[0]) {
    drawText(c, x, y + adv, how, m.small);
    adv += m.gap;
  }
  if (note[0]) {
    drawText(c, x, y + adv, note, m.small);
    adv += m.gap;
  }
  return adv;
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

  // Scrollbar grab (the bar is only live when the content overflows).
  if (_barH > 0.f && fx >= _barX - 6.f && fx <= _barX + _barW + 6.f && fy >= _trackY &&
      fy <= _trackY + _trackH) {
    _dragging = true;
    _dragGrabDy = std::clamp(fy - _barY, 0.f, _barH);
    dragTo(py, winW, winH);
    return true;
  }

  // Tabs: a top strip on narrow windows, otherwise the left column.
  for (int i = 0; i < (int)BookCategory::COUNT; i++) {
    if (L.tabsOnTop) {
      const float tw = (L.contentW - 4.f * 6.f) / (float)BookCategory::COUNT;
      const float tx = L.sideX + i * (tw + 6.f);
      if (fx >= tx && fx <= tx + tw && fy >= L.sideY && fy <= L.sideY + L.tabH) {
        setCategory(i);
        return true;
      }
    } else {
      const float ry = L.sideY + i * L.rowH;
      if (fx >= L.sideX - 6.f && fx <= L.sideX + L.sideW && fy >= ry && fy < ry + L.rowH - 4.f) {
        setCategory(i);
        return true;
      }
    }
  }
  // Everything else inside the panel is swallowed (the book is modal).
  return fx >= L.x0 && fx <= L.x0 + L.w && fy >= L.y0 && fy <= L.y0 + L.h;
}

void RecipeBook::dragTo(int py, int winW, int winH) {
  if (!_dragging) return;
  const float span = _trackH - _barH;
  if (span <= 0.f || _contentH <= _viewH) return;
  const float frac = std::clamp(((float)py - _dragGrabDy - _trackY) / span, 0.f, 1.f);
  _scroll = frac * (_contentH - _viewH);
  (void)winW;
  (void)winH;
}

void RecipeBook::endDrag() { _dragging = false; }

void RecipeBook::draw(GameView &view, SkCanvas *canvas, int winW, int winH) {
  if (!_open || !canvas) return;
  const GameModel &model = view._model;
  const InteractConfig &ic = model.interactConfig();
  const BookState &bs = model.book();
  const Layout L = layoutFor(winW, winH);
  const Metrics &m = L.m;
  const float o = m.o;
  const float g = m.gap;
  const float cell = m.cell;
  const float iconR = m.icon;

  canvas->resetMatrix();
  fillRect(canvas, L.x0, L.y0, L.w, L.h, SkColorSetARGB(252, 14, 16, 22), 10.f);
  strokeRect(canvas, L.x0, L.y0, L.w, L.h, SkColorSetARGB(200, 104, 116, 142), 10.f);

  // ── Title (hint text drops out when there is no room for it) ─────────────
  const float titleScale = std::clamp(2.0f * o, 1.5f, 2.4f);
  drawText(canvas, L.x0 + 14.f * o, L.y0 + 12.f * o, "RECIPE BOOK", titleScale);
  const char *hintFull = "F2 close   1-5 tabs   scroll: wheel / arrows / drag the bar";
  const char *hintShort = "F2 close   1-5 tabs";
  const float legendW = textWidth("green row = done", m.small);
  const float titleEnd = L.x0 + 14.f * o + textWidth("RECIPE BOOK", titleScale) + 16.f * o;
  const float rightEdge = L.x0 + L.w - 14.f * o;
  // Right-hand items are laid out relative to each other (legend first, hint to
  // its left), never both right-aligned to the same edge — that overlaps them.
  const bool legendFits = (rightEdge - legendW > titleEnd);
  if (legendFits)
    drawText(canvas, rightEdge - legendW, L.y0 + 15.f * o, "green row = done", m.small);
  const float hintRight = legendFits ? (rightEdge - legendW - 16.f * o) : rightEdge;
  // Graduated hint: full -> keys only -> nothing, so small windows keep the
  // essential keys instead of dropping the line entirely.
  const char *hint = nullptr;
  if (hintRight - textWidth(hintFull, m.small) > titleEnd + 12.f) hint = hintFull;
  else if (hintRight - textWidth(hintShort, m.small) > titleEnd + 12.f) hint = hintShort;
  if (hint) drawText(canvas, hintRight - textWidth(hint, m.small), L.y0 + 15.f * o, hint, m.small);

  // ── Tabs ─────────────────────────────────────────────────────────────────
  if (L.tabsOnTop) {
    const float tw = (L.contentW - 4.f * 6.f) / (float)BookCategory::COUNT;
    for (int i = 0; i < (int)BookCategory::COUNT; i++) {
      const float tx = L.sideX + i * (tw + 6.f);
      const bool active = (i == _cat);
      fillRect(canvas, tx, L.sideY, tw, L.tabH,
               active ? SkColorSetARGB(235, 52, 62, 84) : SkColorSetARGB(150, 28, 32, 42), 6.f);
      if (active)
        fillRect(canvas, tx, L.sideY + L.tabH - 3.f, tw, 3.f, SkColorSetARGB(255, 132, 176, 255),
                 1.f);
      char label[64];
      const int total = catTotal(ic, (BookCategory)i);
      snprintf(label, sizeof label, "%d %s %d/%d", i + 1, kCatName[i], catDone(bs, (BookCategory)i),
               total);
      drawText(canvas, tx + 8.f * o, L.sideY + L.tabH * 0.5f - 8.f * o, label, m.small);
    }
  } else {
    for (int i = 0; i < (int)BookCategory::COUNT; i++) {
      const BookCategory cat = (BookCategory)i;
      const float ry = L.sideY + i * L.rowH;
      const bool active = (i == _cat);
      if (active) {
        fillRect(canvas, L.sideX - 6.f, ry - 2.f, L.sideW, L.rowH - 4.f,
                 SkColorSetARGB(230, 52, 62, 84), 6.f);
        fillRect(canvas, L.sideX - 6.f, ry - 2.f, 4.f, L.rowH - 4.f,
                 SkColorSetARGB(255, 132, 176, 255), 2.f);
      }
      char label[64];
      snprintf(label, sizeof label, "%d  %s", i + 1, kCatName[i]);
      drawText(canvas, L.sideX + 6.f, ry + 6.f * o, label, m.text);
      const int total = catTotal(ic, cat);
      snprintf(label, sizeof label, "%d/%d", catDone(bs, cat), total);
      drawText(canvas, L.sideX + L.sideW - textWidth(label, m.small) - 10.f, ry + 9.f * o, label,
               m.small);
    }
    strokeRect(canvas, L.sideX + L.sideW + 6.f, L.sideY - 4.f, 1.f, L.contentH + 8.f,
               SkColorSetARGB(90, 110, 120, 140), 0.f);
  }

  // ── Content (clipped, scrolled by _scroll) ───────────────────────────────
  canvas->save();
  canvas->clipRect(SkRect::MakeXYWH(L.contentX, L.contentY, L.contentW, L.contentH));
  const float top = L.contentY;
  float y = top - _scroll;
  const float cx = L.contentX;
  const float cw = L.contentW;
  const SkColor cellBg = SkColorSetARGB(190, 46, 52, 66);
  // When the pane is narrow, wide rows put their result/text block *below* the
  // shape instead of beside it so nothing gets squeezed off the edge.
  const bool narrow = (cw < 470.f * o);
  const bool narrowText = (cw < 560.f * o);
  char buf[128];

  switch ((BookCategory)_cat) {
    case BookCategory::Recipes: {
      drawText(canvas, cx, y, "Shaped patterns (offsets exactly as drawn)", m.small);
      y += g + 2.f;
      y += drawCatHelp(canvas, cx, y, BookCategory::Recipes, m);
      y += 10.f;
      for (int i = 0; i < (int)ic.crafting.size(); i++) {
        const CraftingRule &r = ic.crafting[i];
        const int rows = r.maxDr - r.minDr + 1, cols = r.maxDc - r.minDc + 1;
        // At least 2 grid rows tall so the list reads as an even stack.
        const float shapeH = std::max(2, rows) * cell;
        const float eh = shapeH + (narrow ? cell + 22.f * o : 14.f * o) + 10.f * o;
        const bool done = bs.has(BookCategory::Recipes, i);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f * o, y + 14.f * o, 4.5f * o, done);
          const float gx = cx + 26.f * o, gy = y + 8.f * o;
          for (int rr = 0; rr < rows; rr++) {
            for (int cc = 0; cc < cols; cc++) {
              const float px = gx + cc * cell, py = gy + rr * cell;
              int itemId = -1;
              for (const CraftEntry &e : r.entries)
                if (e.dr - r.minDr == rr && e.dc - r.minDc == cc) { itemId = e.itemId; break; }
              if (itemId < 0) {
                // Empty slot: visible on purpose — where a cell is empty is part
                // of the shape (an L and a 2x2 block differ by exactly this).
                fillRect(canvas, px + 1.f, py + 1.f, cell - 2.f, cell - 2.f,
                         SkColorSetARGB(120, 26, 30, 40), 4.f);
                strokeRect(canvas, px + 1.f, py + 1.f, cell - 2.f, cell - 2.f,
                           SkColorSetARGB(130, 108, 118, 138), 4.f);
                continue;
              }
              fillRect(canvas, px, py, cell, cell, cellBg, 5.f);
              iconAt(view, canvas, px + cell * 0.5f, py + cell * 0.5f, iconR, itemId);
            }
          }
          // Result: beside the shape when there is room, below it when narrow.
          const float ax = narrow ? gx : gx + cols * cell + 14.f * o;
          const float ay = narrow ? gy + rows * cell + 6.f * o : y + 8.f * o;
          drawText(canvas, ax, ay + 4.f * o, "->", m.text);
          const float rx = ax + 30.f * o;
          fillRect(canvas, rx, ay, cell, cell, cellBg, 5.f);
          iconAt(view, canvas, rx + cell * 0.5f, ay + cell * 0.5f, iconR, r.result);
          snprintf(buf, sizeof buf, "%s x%d", view.itemName(r.result).c_str(), r.resultCount);
          drawText(canvas, rx + cell + 8.f * o, ay + 2.f * o, buf, m.text);
          snprintf(buf, sizeof buf, "recipe %d", i);
          drawText(canvas, rx + cell + 8.f * o, ay + 20.f * o, buf, m.small);
          rowStatus(canvas, cx + cw - 12.f * o, y + 6.f * o, done, m.small);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Machines: {
      drawText(canvas, cx, y, "Machine footprints, roles and timings", m.small);
      y += g + 2.f;
      y += drawCatHelp(canvas, cx, y, BookCategory::Machines, m);
      y += 10.f;
      for (int i = 0; i < (int)ic.stationRecipes.size(); i++) {
        const StationRecipe &r = ic.stationRecipes[i];
        const float footH = std::max(1, r.sizeR) * cell;
        const float textH = 4.f * g;
        const float eh = footH + (narrowText ? textH : 14.f * o) + 10.f * o;
        const bool done = bs.has(BookCategory::Machines, i);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f * o, y + 14.f * o, 4.5f * o, done);
          const float gx = cx + 26.f * o, gy = y + 8.f * o;
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
              fillRect(canvas, gx + cc * cell, gy + rr * cell, cell - 2.f, cell - 2.f, col, 4.f);
            }
          }
          const float tx = narrowText ? gx : gx + r.sizeC * cell + 14.f * o;
          float ty = narrowText ? gy + r.sizeR * cell + 6.f * o : y + 6.f * o;
          const char *na = r.inputA >= 0 ? view.itemName(r.inputA).c_str() : "-";
          const char *nb = r.inputB >= 0 ? view.itemName(r.inputB).c_str() : "-";
          snprintf(buf, sizeof buf, "%s + %s  ->  %s x%d", na, nb,
                   view.itemName(r.output).c_str(), r.outputCount);
          drawText(canvas, tx, ty, buf, m.text);
          ty += g + 4.f;
          snprintf(buf, sizeof buf, "%d ticks   (machine %d, %dx%d)", r.duration, r.id, r.sizeR,
                   r.sizeC);
          drawText(canvas, tx, ty, buf, m.small);
          ty += g;
          drawText(canvas, tx, ty, "violet = machine   green = input", m.small);
          ty += g;
          drawText(canvas, tx, ty, "orange = output   blue = buffer", m.small);
          rowStatus(canvas, cx + cw - 12.f * o, y + 6.f * o, done, m.small);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Reactions: {
      drawText(canvas, cx, y, "Item reactions (pair-wise)", m.small);
      y += g + 2.f;
      y += drawCatHelp(canvas, cx, y, BookCategory::Reactions, m);
      y += 10.f;
      for (int i = 0; i < (int)ic.reactions.size(); i++) {
        const ReactionRule &r = ic.reactions[i];
        const float eh = cell + 18.f * o + 12.f * o;
        const bool done = bs.has(BookCategory::Reactions, i);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(done));
          doneMark(canvas, cx + 12.f * o, y + 14.f * o, 4.5f * o, done);
          float px = cx + 26.f * o;
          const float py = y + 8.f * o;
          const float nameY = py + cell + 3.f * o;
          const float opGap = 10.f * o;
          fillRect(canvas, px, py, cell, cell, cellBg, 5.f);
          iconAt(view, canvas, px + cell * 0.5f, py + cell * 0.5f, iconR, r.itemA);
          drawText(canvas, px, nameY, view.itemName(r.itemA).c_str(), m.small);
          px += cell + opGap;
          drawText(canvas, px, py + 4.f * o, "+", m.text);
          px += 14.f * o;
          fillRect(canvas, px, py, cell, cell, cellBg, 5.f);
          iconAt(view, canvas, px + cell * 0.5f, py + cell * 0.5f, iconR, r.itemB);
          drawText(canvas, px, nameY, view.itemName(r.itemB).c_str(), m.small);
          px += cell + opGap;
          drawText(canvas, px, py + 4.f * o, "->", m.text);
          px += 28.f * o;
          fillRect(canvas, px, py, cell, cell, cellBg, 5.f);
          iconAt(view, canvas, px + cell * 0.5f, py + cell * 0.5f, iconR, r.result);
          snprintf(buf, sizeof buf, "%s x%d", view.itemName(r.result).c_str(), r.resultCount);
          drawText(canvas, px + cell + 8.f * o, py + 4.f * o, buf, m.text);
          drawText(canvas, px, nameY, "result", m.small);
          rowStatus(canvas, cx + cw - 12.f * o, py + 4.f * o, done, m.small);
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Stacking: {
      drawText(canvas, cx, y, "How piles behave on their own (tick-driven)", m.small);
      y += g + 2.f;
      y += drawCatHelp(canvas, cx, y, BookCategory::Stacking, m);
      y += 10.f;
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
        const int lines = 2 + (e.desc2[0] ? 1 : 0) + 1;   // desc (+desc2) + how
        const float eh = lines * g + 16.f * o;
        const bool done = bs.has(BookCategory::Stacking, (int)e.id);
        const bool on = physicsEnabled(model, e.id);
        if (y + eh > top && y < top + L.contentH) {
          fillRect(canvas, cx, y, cw, eh - 6.f,
                   on ? rowColor(done) : SkColorSetARGB(150, 24, 26, 32));
          if (on) {
            doneMark(canvas, cx + 12.f * o, y + 13.f * o, 4.5f * o, done);
          } else {   // hollow grey mark = disabled, not "not yet done"
            SkPaint p;
            p.setAntiAlias(true);
            p.setStyle(SkPaint::kStroke_Style);
            p.setStrokeWidth(1.5f);
            p.setColor(SkColorSetARGB(160, 120, 128, 142));
            canvas->drawCircle(cx + 12.f * o, y + 13.f * o, 4.5f * o, p);
          }
          float ty = y + 6.f * o;
          drawText(canvas, cx + 26.f * o, ty, e.title, m.text);
          ty += g + 2.f;
          drawText(canvas, cx + 26.f * o, ty, e.desc, m.small);
          ty += g;
          if (e.desc2[0]) {
            drawText(canvas, cx + 26.f * o, ty, e.desc2, m.small);
            ty += g;
          }
          drawText(canvas, cx + 26.f * o, ty, kPhysicsHow[(int)e.id], m.small);
          if (on) {
            rowStatus(canvas, cx + cw - 12.f * o, y + 6.f * o, done, m.small);
          } else {
            drawText(canvas, cx + cw - 12.f * o - textWidth("off", m.small), y + 6.f * o, "off",
                     m.small);
          }
        }
        y += eh;
      }
      break;
    }
    case BookCategory::Quantum: {
      drawText(canvas, cx, y, "Heat - the only way matter appears from nothing", m.small);
      y += g + 2.f;
      y += drawCatHelp(canvas, cx, y, BookCategory::Quantum, m);
      y += 10.f;
      const float bh = std::clamp(18.f * o, 12.f, 22.f);
      const float barTop = 58.f * o;
      const float eh = barTop + bh + 26.f + 3.f * g + 14.f * o;
      const bool qDone = bs.has(BookCategory::Quantum, (int)BookPhysics::Quantum);
      if (y + eh > top && y < top + L.contentH) {
        fillRect(canvas, cx, y, cw, eh - 6.f, rowColor(qDone));
        doneMark(canvas, cx + 12.f * o, y + 13.f * o, 4.5f * o, qDone);
        drawText(canvas, cx + 26.f * o, y + 6.f * o, "Quantum fluctuation", m.text);
        rowStatus(canvas, cx + cw - 12.f * o, y + 6.f * o, qDone, m.small);
        drawText(canvas, cx + 26.f * o, y + 26.f * o, kPhysicsHow[(int)BookPhysics::Quantum],
                 m.small);
        // Heat bar 0..255, full available width, threshold marked.
        const float bx = cx + 26.f * o;
        const float bw = std::max(120.f, std::min(cw - 52.f * o, 700.f * o));
        const float by = y + barTop;
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
        drawText(canvas, tx + 6.f, by - 20.f * o, buf, m.small);
        drawText(canvas, bx, by + bh + 6.f, "0", m.small);
        drawText(canvas, bx + bw - 22.f, by + bh + 6.f, "255", m.small);
        float ty = by + bh + 26.f;
        snprintf(buf, sizeof buf, "Holding pumps +%d heat per tick.",
                 Heat::QUANTUM_HOLD_HEAT_PER_TICK);
        drawText(canvas, bx, ty, buf, m.small);
        ty += g;
        snprintf(buf, sizeof buf, "An EMPTY cell at %d+ heat rolls %d%% per heat pass (~1.6 s)",
                 Heat::QUANTUM_MIN_HEAT, Heat::QUANTUM_CHANCE_PCT);
        drawText(canvas, bx, ty, buf, m.small);
        ty += g;
        drawText(canvas, bx, ty, "and births a random item; the birth absorbs the heat.", m.small);
        ty += g;
        drawText(canvas, bx, ty, "Hot cells make items act up to 5x more often.", m.small);
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

  // ── Scroll view indicator (drag it, or use the wheel / arrows) ───────────
  _barH = 0.f;
  if (_contentH > _viewH + 1.f) {
    const float barW = std::clamp(6.f * o, 4.f, 10.f);
    _trackY = L.contentY;
    _trackH = L.contentH;
    _barX = L.contentX + L.contentW + 2.f;
    _barW = barW;
    const float frac = _viewH / _contentH;
    _barH = std::max(28.f * o, _trackH * frac);
    _barY = _trackY + (_trackH - _barH) * (_scroll / (_contentH - _viewH));
    fillRect(canvas, _barX, _trackY, barW, _trackH, SkColorSetARGB(80, 110, 120, 140), 2.f);
    fillRect(canvas, _barX, _barY, barW, _barH,
             _dragging ? SkColorSetARGB(240, 190, 205, 235) : SkColorSetARGB(210, 150, 160, 180),
             2.f);
  }
}
