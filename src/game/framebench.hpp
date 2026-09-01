#pragma once
// Per-frame render-stage timing, accumulated by GameView during renderTo so a
// benchmark can break the frame budget down by what Skia is doing. Reset per
// frame by the driver, read back after renderTo returns.
//
// Kept in its own header so the text layer (font.cpp) can accumulate text-stage
// timings without pulling in the whole GameView class.
struct Framebench {
  double cellsMs = 0.0;   // tile geometry + culling walk (renderAnchor), net of the sub-stages below
  double glowMs = 0.0;    // item glow radial gradients
  double svgMs = 0.0;     // per-instance chess-piece SVG rendering
  int    svgN = 0;        // how many SVG pieces drawn this frame
  double textMs = 0.0;          // vector-font badge text (metrics+blob+draw)
  double textMetricsMs = 0.0;   //   └─ textWidth/textHeight (measure/metrics)
  double textBlobMs = 0.0;      //   └─ SkTextBlob::MakeFromString (glyph runs)
  double textDrawMs = 0.0;      //   └─ canvas->drawTextBlob
  int    textN = 0;             // how many drawText (badge) draws this frame
  int    textMetricsN = 0;      // how many width/height measures this frame
  int    textBlobN = 0;         // how many blobs built this frame
  int    cellN = 0;             // how many cells were visited (culled or drawn)
  void reset() {
    cellsMs = 0.0;
    glowMs = 0.0; svgMs = 0.0; svgN = 0;
    textMs = 0.0; textMetricsMs = 0.0; textBlobMs = 0.0; textDrawMs = 0.0;
    textN = 0; textMetricsN = 0; textBlobN = 0;
    cellN = 0;
  }
};
