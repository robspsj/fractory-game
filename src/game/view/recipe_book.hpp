#pragma once
#include "../model/book_state.hpp"

class GameView;
class SkCanvas;

// In-game recipe book (docs/recipe-book.md): a tabbed overlay listing every
// transformation the simulation knows about — crafting shapes, machine
// footprints, item reactions, stack behaviour and heat/quantum physics — with
// the entries the player has already performed marked.
//
// Every entry is generated from the loaded tables (InteractConfig) and the
// balancing constants, so the book can never drift from the simulation. Entries
// are addressed by (category, row index), which is also the id space
// BookState::mark() records progress in — that is the hook for making entries
// clickable later.
class RecipeBook {
 public:
  bool isOpen() const { return _open; }
  void toggle() { _open = !_open; }
  void close() { _open = false; }
  int category() const { return _cat; }
  void setCategory(int c);
  void cycle(int dir);
  void scrollBy(float dy);
  // Mouse click in pixels. Returns true when the book consumed the click
  // (tab hit, scrollbar grab, or inside the panel).
  bool click(int px, int py, int winW, int winH);
  // Mouse motion while the button is held: drags the scrollbar if it was
  // grabbed by click(). No-op otherwise.
  void dragTo(int py, int winW, int winH);
  void endDrag();
  // Draw the overlay (pixel space; the caller's canvas is left untouched).
  void draw(GameView &view, SkCanvas *canvas, int winW, int winH);

 private:
  bool _open = false;
  int _cat = 0;
  float _scroll = 0.f;
  // Measured on the last draw so scrolling can clamp to the content.
  float _contentH = 0.f;
  float _viewH = 1.f;
  // Scroll view geometry, measured on the last draw so click() can hit-test the
  // bar and dragTo() can map the mouse to a scroll position.
  float _barX = 0.f, _barY = 0.f, _barW = 0.f, _barH = 0.f;
  float _trackY = 0.f, _trackH = 0.f;
  bool _dragging = false;
  float _dragGrabDy = 0.f;
};
