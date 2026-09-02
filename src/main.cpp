#include "font.hpp"
#include "game/config.hpp"
#include "game/game.hpp"
#include "test_runner.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

static std::unique_ptr<Game> s_game;
static int winW = 800, winH = 600;
static uint64_t s_lastFrameMs = 0;
static int s_frameCount = 0;
static uint64_t s_lastFpsTime = 0;
static float s_fps = 0.0f;

// Frame-time breakdown (ms), smoothed via EMA for stable HUD numbers.
static float s_msFrame = 0.0f;   // full measured frame (render->present)
static float s_msStep = 0.0f;    // Game::step (model tick + particles + effects)
static float s_msRender = 0.0f;  // Skia render + HUD
static float s_msCopy = 0.0f;    // surface binding / buffer wrap
static float s_msPresent = 0.0f; // present (SDL surface blit / canvas putImageData)
static uint64_t s_lastProfileTick = 0;
static uint64_t s_perfFreq = 0;
static bool s_vsyncOn = false;

// ---- Live frame profiler (F3 toggles the on-screen bar panel) ---------------
// Per-stage render timings are captured once per frame from GameView's
// Framebench (which the render pass fills), EMA-smoothed for stable bars, and
// drawn as a bar chart so you can WATCH which stage grows as you zoom. The
// stage that expands with zoom level is your culprit.
struct ProfilerStat {
  double stepMs = 0.0;     // model step (zoom-independent — sanity baseline)
  double renderMs = 0.0;   // total renderTo
  double cellsMs = 0.0;    // geometry/cells/other (render minus svg+glow+text)
  double svgMs = 0.0;      // per-instance piece SVG rendering
  double glowMs = 0.0;     // item contour glow
  double textMs = 0.0;     // badge vector-font text
  double presentMs = 0.0;  // SDL blit / putImageData
  double frameMs = 0.0;    // full measured frame
  int svgN = 0;            // piece SVGs drawn this frame
  int textN = 0;           // badges drawn this frame
  int cells = 0;           // cells drawn this frame (draw count)
  int depth = 0;           // anchor recursion depth
  float zoom = 1.0f;
  int fps = 0;
};
static ProfilerStat s_prof;            // EMA-smoothed values shown by the panel
static bool s_profilerOn = false;      // F3 toggles
static int s_profW = 0, s_profH = 0;   // last window size (panel is bottom-left)

static void profilerCapture() {
  // Pull the just-finished frame's per-stage timings. renderTo() filled the
  // view's Framebench; read it before the next frame resets it.
  const Framebench& fb = s_game->view().framebench();
  const Game::StepProfile sp = s_game->stepProfile();
  double frameTotal = (double)s_msFrame;
  // "cells/other" = render minus the individually-timed stages.
  double cells = (double)s_msRender - fb.svgMs - fb.glowMs - fb.textMs;
  if (cells < 0.0) cells = 0.0;
  // Smooth each channel the same way the existing ms* HUD numbers do.
  auto e = [](double prev, double cur) { return prev + 0.2 * (cur - prev); };
  // Model step plotted as PER-TICK cost (tickSum / ticks this frame), not the
  // per-frame sum: a fixed 20 Hz tick rate means slower (heavier-render) frames
  // pack MORE ticks into them, so the raw sum rises with zoom even though each
  // tick costs the same. Dividing out the tick count keeps this bar flat — the
  // true view-independent baseline.
  const double perTick = sp.ticks > 0 ? sp.tickMs / sp.ticks : 0.0;
  s_prof.stepMs    = e(s_prof.stepMs, perTick);
  s_prof.renderMs  = e(s_prof.renderMs, (double)s_msRender);
  s_prof.cellsMs   = e(s_prof.cellsMs, cells);
  s_prof.svgMs     = e(s_prof.svgMs, fb.svgMs);
  s_prof.glowMs    = e(s_prof.glowMs, fb.glowMs);
  s_prof.textMs    = e(s_prof.textMs, fb.textMs);
  s_prof.presentMs = e(s_prof.presentMs, (double)s_msPresent);
  s_prof.frameMs   = e(s_prof.frameMs, frameTotal);
  s_prof.svgN  = fb.svgN;
  s_prof.textN = fb.textN;
  s_prof.cells = s_game->lastCellDrawCount();
  s_prof.depth = s_game->anchorDepth();
  s_prof.zoom  = s_game->zoomFactor();
  s_prof.fps   = (int)(s_fps + 0.5f);
}

#if !defined(__EMSCRIPTEN__)
static bool s_gpuActive = false;   // true when the Metal Ganesh path is live
#endif

static float ema(float prev, float cur) {
  const float a = 0.2f;
  return prev + a * (cur - prev);
}

static uint64_t perfNow() { return SDL_GetPerformanceCounter(); }
static double perfMs(uint64_t a, uint64_t b) {
  return (double)(b - a) / (double)s_perfFreq * 1000.0;
}

#if !defined(__EMSCRIPTEN__)
static void drawHud(SkCanvas*);   // defined below; used by the GPU present path
// GPU glue lives in metal_gpu.cpp (isolated so the Skia GPU headers don't clash
// with the game's `Rect`). main.cpp only drives it via these C entry points.
extern "C" {
bool frct_gpu_init(void*, bool);
bool frct_gpu_frame(void (*render)(void*, SkCanvas*, int, int), void* ctx);
void frct_gpu_shutdown(void);
}
static bool gpuSetup(SDL_Window* win) { return (s_gpuActive = frct_gpu_init(win, s_vsyncOn)); }
static void gpuRenderCb(void*, SkCanvas* canvas, int w, int h) {
  s_game->renderTo(canvas, w, h);
  drawHud(canvas);
}
static bool gpuFrame() { return frct_gpu_frame(gpuRenderCb, nullptr); }
#endif

// Web RGBA8888 backing buffer (presented via canvas putImageData). Native draws
// straight into the SDL window surface, so this is only used by the web build.
static std::vector<uint8_t> s_rgba;
static int s_rgbaW = 0, s_rgbaH = 0;

static void updateFps() {
  s_frameCount++;
  uint32_t now = SDL_GetTicks();
  if (now - s_lastFpsTime >= 200) {
    s_fps = s_frameCount * 1000.0f / (float)(now - s_lastFpsTime);
    s_frameCount = 0;
    s_lastFpsTime = now;
  }
}

// ---------------------------------------------------------------------------
// Live frame profiler — an on-screen bar chart (F3 toggles it) showing where
// the frame time goes, per stage. Watch the bars while you ZOOM: the stage
// whose bar grows with zoom level is your culprit. Uses the same primitives as
// the HUD (custom drawText + SkPaint hex-color rects).
// ---------------------------------------------------------------------------
static void drawProfiler(SkCanvas *canvas) {
  if (!s_profilerOn || !canvas) return;
  canvas->resetMatrix();
  const float fs = 1.4f;             // text scale (1.0 = 8px HUD glyphs)
  const float rowPitch = 22.0f;      // vertical gap between bar rows
  const float left = 12.0f, top = 14.0f;
  const float barX = 152.0f;         // x where value bars start
  const float barWMax = 184.0f;      // full bar width == frameMs
  const float headerH = 48.0f;       // room for title + hint, above the bars
  const int nBars = 6;

  const float panelW = barX + barWMax + 88.0f;
  const float panelH = top + headerH + (nBars + 1) * rowPitch + 14.0f;
  {
    SkPaint bg;
    bg.setAntiAlias(false);
    bg.setColor(0xD010141B);        // dark translucent panel
    canvas->drawRect(SkRect::MakeXYWH(0, 0, panelW, panelH), bg);
  }

  const float scale = barWMax / (s_prof.frameMs > 1e-3 ? s_prof.frameMs : 1.0);
  char line[96];
  snprintf(line, sizeof(line), "%d fps   %.1f ms   zoom x%.2f   cells %d  svg %d  badge %d",
           s_prof.fps, s_prof.frameMs, s_prof.zoom, s_prof.cells, s_prof.svgN, s_prof.textN);
  drawText(canvas, left, top + 12, line, fs);
  snprintf(line, sizeof(line), "which bar grows as you ZOOM?");
  drawText(canvas, left, top + 12 + 17, line, fs);

  struct Row { const char *label; double ms; uint32_t col; };
  const Row rows[nBars] = {
    { "model step", s_prof.stepMs,    0xFF9C5555 },
    { "cells/geom", s_prof.cellsMs,   0xFF4C8CB3 },
    { "piece SVG",  s_prof.svgMs,     0xFF9A6CBE },
    { "glow",       s_prof.glowMs,    0xFFBF9A40 },
    { "badge text", s_prof.textMs,    0xFF73BF73 },
    { "present",    s_prof.presentMs, 0xFF808C99 },
  };
  // Vertical centering: a 14px bar over vector-font text whose tight
  // height is textHeight(fs) with its top at y.  y-11 was valid for the old
  // 8×8 bitmap font (8*fs = 11.2px, bar 14 => offset -1.4px looked like -11
  // only because the bitmap origin differed); with the vector font the text's
  // top is at y + |ascent| baseline correction, so the bar must be centered
  // in the text box: barY = y + (th - barH)/2.
  const float th = textHeight(fs);
  const float barH = 14.0f;
  const float barYO = (th - barH) * 0.5f; // add to y to get bar top
  float y = top + headerH;
  for (int i = 0; i < nBars; i++) {
    drawText(canvas, left, y, rows[i].label, fs);
    const float barY = y + barYO;
    // track
    {
      SkPaint tr;
      tr.setAntiAlias(false);
      tr.setColor(0x14FFFFFF);
      canvas->drawRect(SkRect::MakeXYWH(barX, barY, barWMax, barH), tr);
    }
    // bar
    if (rows[i].ms > 0.01) {
      float bw = (float)(rows[i].ms * scale);
      if (bw > barWMax) bw = barWMax;
      SkPaint bp;
      bp.setAntiAlias(false);
      bp.setColor(rows[i].col);
      canvas->drawRect(SkRect::MakeXYWH(barX, barY, bw, barH), bp);
    }
    // value
    char v[24];
    snprintf(v, sizeof(v), "%.2fms", rows[i].ms);
    drawText(canvas, barX + barWMax + 6, y, v, fs);
    y += rowPitch;
  }

  // Stacked render-breakdown bar (renderTo only): the render stage, split.
  {
    y += 6.0f;
    drawText(canvas, left, y, "render", fs);
    const float barY = y + barYO;
    const double rref = s_prof.renderMs > 1e-3 ? s_prof.renderMs : 1.0;
    const float rscale = barWMax / (float)rref;
    const Row sub[] = {
      { "c", s_prof.cellsMs, 0xFF4C8CB3 },
      { "s", s_prof.svgMs,   0xFF9A6CBE },
      { "g", s_prof.glowMs,  0xFFBF9A40 },
      { "t", s_prof.textMs,  0xFF73BF73 },
    };
    float x = barX;
    for (int i = 0; i < 4; i++) {
      if (sub[i].ms <= 0.0) continue;
      float bw = (float)(sub[i].ms * rscale);
      if (bw < 0.5f) bw = 0.5f;
      if (x + bw > barX + barWMax) bw = (barX + barWMax) - x;
      SkPaint bp;
      bp.setAntiAlias(false);
      bp.setColor(sub[i].col);
      canvas->drawRect(SkRect::MakeXYWH(x, barY, bw, barH), bp);
      x += bw;
    }
    char v[24];
    snprintf(v, sizeof(v), "%.2fms", s_prof.renderMs);
    drawText(canvas, barX + barWMax + 6, y, v, fs);
  }
}

// Pull this frame's stage timings out of GameView's Framebench into the
// smoothed s_prof (exponentially averaged so the on-screen bars don't jitter),
// plus the current zoom + cell count for the headline. Called from drawHud,
// which runs in every render path (native CPU, GPU/Metal, web) right after
// renderTo — so the profiler is correct everywhere.
static void captureFrameStats() {
  const Framebench &b = s_game->view().framebench();
  s_prof.fps       = s_fps;
  s_prof.zoom      = (double)s_game->zoomFactor();
  s_prof.cells     = b.cellN;
  s_prof.svgN      = b.svgN;
  s_prof.textN     = b.textN;
  s_prof.stepMs    = ema(s_prof.stepMs,    (float)s_msStep);
  s_prof.renderMs  = ema(s_prof.renderMs,  (float)s_msRender);
  s_prof.presentMs = ema(s_prof.presentMs, (float)s_msPresent);
  s_prof.frameMs   = ema(s_prof.frameMs,   (float)s_msFrame);
  s_prof.cellsMs   = ema(s_prof.cellsMs,   (float)b.cellsMs);
  s_prof.svgMs     = ema(s_prof.svgMs,     (float)b.svgMs);
  s_prof.glowMs    = ema(s_prof.glowMs,    (float)b.glowMs);
  s_prof.textMs    = ema(s_prof.textMs,    (float)b.textMs);
}

// Overlay the HUD. The caller's canvas carries the game's NDC matrix; we reset
// to an identity (pixel) matrix before drawing the monospace 8x8 font.
static void drawHud(SkCanvas *canvas) {
  updateFps();
  captureFrameStats();
  canvas->resetMatrix();
  int disp = (int)(s_fps + 0.5f);
  const auto sp = s_game->stepProfile();   // last step()'s per-stage timing
  const auto &tp = sp.tick;
  char t[12][72];
  snprintf(t[0],  72, "FPS: %d  %c", disp, s_vsyncOn ? 'V' : 'v');
  snprintf(t[1],  72, "Step: %.2fms  tick %.2f x%d | parts %.2f | eff %.2f",
           (double)s_msStep, sp.tickMs, sp.ticks, sp.partsMs, sp.effectsMs);
  snprintf(t[2],  72, "  tick: sweep %.2f  nodes=%d  items=%d  spills=%d",
           tp.sweepMs, tp.nodes, tp.items, tp.spills);
  snprintf(t[3],  72, "Render: %.2fms  Present: %.2fms",
           (double)s_msRender, (double)s_msPresent);
  snprintf(t[4],  72, "Cells: %d  Depth: %d", s_game->lastCellDrawCount(), s_game->anchorDepth());
  snprintf(t[5],  72, "Zoom: x%.2f", (double)s_game->zoomFactor());
  snprintf(t[6],  72, "res: %dx%d", winW, winH);
  snprintf(t[7],  72, "v%s", FRACTORY_VERSION);
  {
    std::size_t mem = s_game->allocatedMemory();
    const char* units[] = {"B", "KB", "MB", "GB"};
    int u = 0;
    double v = (double)mem;
    while (v >= 1024.0 && u < 3) { v /= 1024.0; u++; }
    snprintf(t[8], 72, "Mem: %.2f %s", v, units[u]);
  }
  snprintf(t[9], 72, "Sources: %d (off %d)  Drawn: %d  A:%d", s_game->puffSources(),
           s_game->offscreenSources(), s_game->puffsDrawn(), s_game->particleAllocs());
  for (int i = 0; i < 10; i++)
    drawText(canvas, winW - textWidth(t[i], 1) - 4, 4 + i * 10, t[i], 1);
  drawProfiler(canvas);   // F3-toggled live bar chart (no-op when off)
}

#if defined(__EMSCRIPTEN__)
static SDL_Window *s_window;

EM_JS(void, jsPresent, (int w, int h, const uint8_t *ptr), {
  var ctx = Module._canvasCtx;
  var img = ctx.createImageData(w, h);
  var data = img.data;
  data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
  ctx.putImageData(img, 0, 0);
});

static void frame() {
  int w, h;
  SDL_GetWindowSize(s_window, &w, &h);
  if (w > 0 && h > 0 && (w != winW || h != winH)) {
    winW = w; winH = h;
    emscripten_set_canvas_element_size("#canvas", winW, winH);
    if (w != s_rgbaW || h != s_rgbaH) {
      s_rgbaW = w; s_rgbaH = h;
      s_rgba.resize((size_t)w * h * 4);
    }
  }

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT)
      emscripten_cancel_main_loop();
    // mouse
    if (e.type == SDL_EVENT_MOUSE_MOTION)
      s_game->update((int)e.motion.x, (int)e.motion.y, winW, winH);
    else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      int mod = (SDL_GetModState() & SDL_KMOD_SHIFT) ? 1 : 0;
      s_game->mouseDown(e.button.button, mod, (int)e.button.x, (int)e.button.y, winW, winH);
    } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
      s_game->mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
    else if (e.type == SDL_EVENT_MOUSE_WHEEL)
      s_game->mouseWheel((float)e.wheel.x, (float)e.wheel.y, (int)e.wheel.mouse_x, (int)e.wheel.mouse_y, winW, winH);
    else if (e.type == SDL_EVENT_KEY_DOWN) {
      Key k = Key::UNKNOWN;
      if (e.key.key == SDLK_F1) k = Key::F1;
      bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
      s_game->keyDown(k, shift, winW, winH);
    }
  }

  uint64_t now = SDL_GetTicks();
  s_game->step((int)(now - s_lastFrameMs));
  s_lastFrameMs = now;

  uint64_t f0 = perfNow();
  SkImageInfo info = SkImageInfo::Make(s_rgbaW, s_rgbaH, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, s_rgba.data(), (size_t)s_rgbaW * 4);
  if (!surf)
    return;
  uint64_t f1 = perfNow();
  SkCanvas *canvas = surf->getCanvas();
  s_game->renderTo(canvas, s_rgbaW, s_rgbaH);
  drawHud(canvas);
  uint64_t f2 = perfNow();
  jsPresent(s_rgbaW, s_rgbaH, s_rgba.data());
  uint64_t f3 = perfNow();

  s_msFrame   = ema(s_msFrame,   (float)perfMs(f0, f3));
  s_msCopy    = ema(s_msCopy,    (float)perfMs(f0, f1));
  s_msRender  = ema(s_msRender,  (float)perfMs(f1, f2));
  s_msPresent = ema(s_msPresent, (float)perfMs(f2, f3));
}

int main() {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  initFont();

  s_window = SDL_CreateWindow("fractory blend2d", 800, 600, SDL_WINDOW_RESIZABLE);
  if (!s_window)
    return 1;

  int w, h;
  SDL_GetWindowSize(s_window, &w, &h);
  winW = w; winH = h;
  s_rgbaW = w; s_rgbaH = h;
  s_rgba.resize((size_t)w * h * 4);
  emscripten_set_canvas_element_size("#canvas", winW, winH);

  EM_ASM({
    Module._canvasCtx = document.querySelector('#canvas').getContext('2d');
  });

  s_game = std::make_unique<Game>(Config{});
#if !defined(__EMSCRIPTEN__)
  s_game->view().loadChessAssets();
  s_game->view().loadUiAssets();
#endif
  s_perfFreq = SDL_GetPerformanceFrequency();
  s_lastFrameMs = SDL_GetTicks();
  s_lastFpsTime = s_lastFrameMs;

  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}

#else // ---- Native SDL3 ----

static SDL_Window *s_window;

static void saveWindowState() {
  int x, y, w, h;
  SDL_GetWindowPosition(s_window, &x, &y);
  SDL_GetWindowSize(s_window, &w, &h);
  FILE *f = fopen("window_state.txt", "w");
  if (f) {
    fprintf(f, "%d %d %d %d\n", x, y, w, h);
    fclose(f);
  }
}

static bool loadWindowState(int &x, int &y, int &w, int &h) {
  FILE *f = fopen("window_state.txt", "r");
  if (!f)
    return false;
  bool ok = fscanf(f, "%d %d %d %d", &x, &y, &w, &h) == 4;
  fclose(f);
  return ok;
}

int main(int argc, char *argv[]) {
  // Headless model tests: `fractory --test <file.csv> [--grid-limit N]` runs the
  // CSV scenario suite and exits 0/1. Renders nothing (pure model logic).
  if (argc >= 3 && std::string(argv[1]) == "--test") {
    int gridLimit = 100;
    for (int a = 3; a + 1 < argc; a++) {
      if (std::string(argv[a]) == "--grid-limit")
        gridLimit = std::atoi(argv[a + 1]);
    }
    bool success = ModelTestRunner::runTest(argv[2], gridLimit);
    return success ? 0 : 1;
  }

  Config cfg;
  // VSync is off by default (unlocked FPS); --vsync re-enables it.
  // --headless <out.ppm> [W H] renders one frame offscreen for deterministic
  // pixel verification (no window, no SDL), writing a PPM (no codec needed).
  // --grid-limit <N>: override Config::gridLimit (subgrid count) for board-size
  // studies. Default 500000 -> ~11.5M nodes; lowering it shrinks the board.
  std::string headlessPath;
  int headW = 800, headH = 600;
  // --bench [W H] [frames]: render N frames offscreen at W x H and report the
  // frame-budget breakdown (step / render / SVG / glow / HUD / dither). No
  // window, no SDL present; present (SDL UpdateWindowSurface) is not included.
  bool benchMode = false;
  int benchW = 3840, benchH = 2049, benchFrames = 5;
  // --tick <ticks>: headless model-tick micro-benchmark (avg tick ms).
  bool tickMode = false;
  int tickCount = 60;
  // --single <out.ppm> <elementIdx> <size>: render ONE icon (piece+glow) centred
  // on a <size>x<size> surface, to measure piece-vs-glow alignment.
  std::string singlePath;
  int singleIdx = 0, singleSize = 600;
  bool useGpu = true;   // native default; --cpu forces the CPU present path
  // --badge-test: force a row of cells with counts 1..10 so the X{count} badge
  // can be visually verified without playing through RNG.
  bool badgeTest = false;
  bool benchFull = false;  // --full: fill EVERY grid cell with count>=5 (worst-case badges)
  int hoverNode = -1; // --hover <nodeIdx>: force a hover highlight on a cell (headless)
  bool profilerHeadless = false; // --profiler: paint the F3 bar-chart panel in the headless frame
  for (int a = 1; a < argc; a++) {
    if (std::string(argv[a]) == "--empty-grid") {
      cfg.emptyGrid = true;
      if (a + 1 < argc && std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos)
        cfg.initialGridSize = std::atoi(argv[++a]);
    } else if (std::string(argv[a]) == "--badge-test") {
      badgeTest = true;
    } else if (std::string(argv[a]) == "--full") {
      benchFull = true;
    } else if (std::string(argv[a]) == "--vsync") {
      cfg.vsync = true;
    } else if (std::string(argv[a]) == "--cpu") {
      useGpu = false;
    } else if (std::string(argv[a]) == "--headless" && a + 1 < argc) {
      headlessPath = argv[++a];
      if (a + 2 < argc &&
          std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos &&
          std::string(argv[a + 2]).find_first_not_of("0123456789") == std::string::npos) {
        headW = std::atoi(argv[a + 1]); headH = std::atoi(argv[a + 2]); a += 2;
      }
    } else if (std::string(argv[a]) == "--bench") {
      benchMode = true;
      if (a + 2 < argc &&
          std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos &&
          std::string(argv[a + 2]).find_first_not_of("0123456789") == std::string::npos) {
        benchW = std::atoi(argv[a + 1]); benchH = std::atoi(argv[a + 2]); a += 2;
      }
      if (a + 1 < argc && std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos)
        benchFrames = std::atoi(argv[++a]);
    } else if (std::string(argv[a]) == "--single" && a + 3 < argc) {
      singlePath = argv[++a];
      singleIdx = std::atoi(argv[++a]);
      singleSize = std::atoi(argv[++a]);
    } else if (std::string(argv[a]) == "--tick") {
      tickMode = true;
      if (a + 1 < argc && std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos)
        tickCount = std::atoi(argv[++a]);
    } else if (std::string(argv[a]) == "--hover" && a + 1 < argc) {
      hoverNode = std::atoi(argv[++a]);
    } else if (std::string(argv[a]) == "--profiler") {
      profilerHeadless = true;
    } else if (std::string(argv[a]) == "--grid-limit" && a + 1 < argc) {
      if (std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos)
        cfg.gridLimit = std::atoi(argv[++a]);
    }
  }

  if (tickMode) {
    // Headless model-tick micro-benchmark: avg time per GameModel::tick() on the
    // auto-generated config, useful to measure sweep parallelization speedup.
    Game g(cfg);
    (void)g.benchModelTicks(1);   // warm-up (caches, first-use allocs)
    double ms = g.benchModelTicks(tickCount);
    fprintf(stderr, "[tick] avg=%.3f ms/tick  nodes=%d  (%d ticks)\n",
            ms, g.modelNodeCount(), tickCount);
    // One-shot node-type histogram: what are the 11.5M nodes actually?
    {
      const auto &m = g.model();
      long cnt[5] = {0, 0, 0, 0, 0}; // EMPTY,ITEM,GRID,STATION,RESERVED
      int maxDepth = 0;
      for (int i = 0; i < m.totalNodes(); i++) {
        int t = (int)m.node(i).content.type;
        if (t >= 0 && t < 5) cnt[t]++;
        // depth = number of GRID ancestors (walk parentId chain, cheap-ish)
        if (m.node(i).content.type == CellType::GRID) {
          int d = 0, p = m.node(i).parentId;
          while (p > 0) {
            if (m.node(p).content.type == CellType::GRID) d++;
            p = m.node(p).parentId;
          }
          if (d > maxDepth) maxDepth = d;
        }
      }
      fprintf(stderr,
              "[tick] breakdown: EMPTY=%ld ITEM=%ld GRID=%ld STATION=%ld "
              "RESERVED=%ld  (max subgrid depth=%d)\n",
              cnt[0], cnt[1], cnt[2], cnt[3], cnt[4], maxDepth);
      fprintf(stderr,
              "[tick] sizeof(Cell)=%zu  node-array-cap=%.1f MB  (capacity "
              "x sizeof)\n",
              sizeof(Cell),
              (double)m.allocatedBytes() / (1024.0 * 1024.0));
    }
    return 0;
  }

  if (!headlessPath.empty()) {
    Game g(cfg);
#if !defined(__EMSCRIPTEN__)
    g.view().loadChessAssets();
    g.view().loadUiAssets();
#endif
    std::vector<uint8_t> buf((size_t)headW * headH * 4);
    SkImageInfo info = SkImageInfo::Make(headW, headH, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, buf.data(), (size_t)headW * 4);
    if (!surf)
      return 1;
#if !defined(__EMSCRIPTEN__)
    // --badge-test: force the 5x6 grid cells to counts 1..10, rest empty, so we
    // can visually verify the X{count} badge rendering at every size.
    // --full: fill EVERY cell with count>=5 for a worst-case badge board.
    if (badgeTest || benchFull) {
      int gridData[GameModel::GRID * GameModel::GRID * 2];
      for (int i = 0; i < GameModel::GRID * GameModel::GRID; i++) {
        int count;
        if (benchFull)
          count = 5 + (i % 20);
        else if (i < 10)
          count = i + 1;
        else
          count = 0;
        if (count > 0) {
          gridData[i * 2] = i % 3;      // item id cycles 0,1,2
          gridData[i * 2 + 1] = count;
        } else {
          gridData[i * 2] = -1;         // empty
          gridData[i * 2 + 1] = 0;
        }
      }
      g.model().setFullState(gridData);
    }
    if (hoverNode >= 0)
      g.view().setHoveredCellNode(hoverNode);
#endif
    g.renderTo(surf->getCanvas(), headW, headH);
#if !defined(__EMSCRIPTEN__)
    // --profiler: also paint the live F3 bar-chart panel over this frame so it
    // can be visually verified offscreen (uses the just-rendered frame's bench).
    if (profilerHeadless) {
      const Framebench &b = g.view().framebench();
      s_prof.fps = 0; s_prof.zoom = (double)g.zoomFactor();
      s_prof.cells = b.cellN; s_prof.svgN = b.svgN; s_prof.textN = b.textN;
      s_prof.renderMs = b.cellsMs + b.svgMs + b.glowMs + b.textMs;
      s_prof.frameMs  = 24.0;   // nominal, so bars are on a sane scale
      s_prof.stepMs   = 0.0; s_prof.presentMs = 0.0;
      s_prof.cellsMs  = b.cellsMs; s_prof.svgMs = b.svgMs;
      s_prof.glowMs   = b.glowMs; s_prof.textMs = b.textMs;
      s_profilerOn = true;
      drawProfiler(surf->getCanvas());
    }
#endif
    std::string hdr = "P6\n" + std::to_string(headW) + " " + std::to_string(headH) + "\n255\n";
    FILE *f = fopen(headlessPath.c_str(), "wb");
    if (!f)
      return 1;
    fwrite(hdr.c_str(), 1, hdr.size(), f);
    for (int y = 0; y < headH; y++)
      for (int x = 0; x < headW; x++)
        fwrite(&buf[((size_t)y * headW + x) * 4], 1, 3, f); // RGBA -> RGB
    fclose(f);
    fprintf(stderr, "headless frame written to %s (%dx%d)\n",
            headlessPath.c_str(), headW, headH);
    return 0;
  }

  if (!singlePath.empty()) {
    Game g(cfg);
#if !defined(__EMSCRIPTEN__)
    g.view().loadChessAssets();
    g.view().loadUiAssets();
#endif
    const int S = singleSize;
    std::vector<uint8_t> buf((size_t)S * S * 4);
    SkImageInfo info = SkImageInfo::Make(S, S, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, buf.data(), (size_t)S * 4);
    if (!surf)
      return 1;
    g.view().debugIcon(surf->getCanvas(), S, S, singleIdx, 0.9f);
    std::string hdr = "P6\n" + std::to_string(S) + " " + std::to_string(S) + "\n255\n";
    FILE *f = fopen(singlePath.c_str(), "wb");
    if (!f)
      return 1;
    fwrite(hdr.c_str(), 1, hdr.size(), f);
    for (int y = 0; y < S; y++)
      for (int x = 0; x < S; x++)
        fwrite(&buf[((size_t)y * S + x) * 4], 1, 3, f);
    fclose(f);
    fprintf(stderr, "single icon %d -> %s (%dx%d)\n", singleIdx, singlePath.c_str(), S, S);
    return 0;
  }

  if (benchMode) {
    s_game = std::make_unique<Game>(cfg);
    s_perfFreq = SDL_GetPerformanceFrequency();
#if !defined(__EMSCRIPTEN__)
    s_game->view().loadChessAssets();
  s_game->view().loadUiAssets();
    // --bench ... --badge: fill the top-left cells with counts 1..10 so the
    // X{count} badges render during the benchmark and their text cost is
    // measured (otherwise an empty board draws no text at all).
    // --bench ... --full: fill EVERY grid cell with count>=5 → a worst-case
    // full board of badges to stress the text path and watch it scale.
    if (badgeTest || benchFull) {
      int gridData[GameModel::GRID * GameModel::GRID * 2];
      for (int i = 0; i < GameModel::GRID * GameModel::GRID; i++) {
        int count;
        if (benchFull)
          count = 5 + (i % 20);        // 5..24, every cell has a badge
        else if (i < 10)
          count = i + 1;               // 1..10
        else
          count = 0;
        if (count > 0) {
          gridData[i * 2] = i % 3;      // item id cycles 0,1,2
          gridData[i * 2 + 1] = count;
        } else {
          gridData[i * 2] = -1;
          gridData[i * 2 + 1] = 0;
        }
      }
      s_game->model().setFullState(gridData);
    }
#endif
    std::vector<uint8_t> buf((size_t)benchW * benchH * 4);
    SkImageInfo info = SkImageInfo::Make(benchW, benchH, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType);
    sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, buf.data(), (size_t)benchW * 4);
    if (!surf)
      return 1;
    double stepMs = 0, renderMs = 0, hudMs = 0, svgMs = 0, glowMs = 0;
    double textMs = 0, textMetricsMs = 0, textBlobMs = 0, textDrawMs = 0;
    long svgN = 0, textN = 0, textMetricsN = 0, textBlobN = 0;
    for (int f = 0; f < benchFrames; f++) {
      s_game->view().resetFramebench();
      uint64_t t;
      t = perfNow(); s_game->step(16); stepMs += perfMs(t, perfNow());
      t = perfNow(); s_game->renderTo(surf->getCanvas(), benchW, benchH); renderMs += perfMs(t, perfNow());
      const Framebench& fb = s_game->view().framebench();
      svgMs += fb.svgMs; glowMs += fb.glowMs; svgN += fb.svgN;
      textMs += fb.textMs; textMetricsMs += fb.textMetricsMs;
      textBlobMs += fb.textBlobMs; textDrawMs += fb.textDrawMs;
      textN += fb.textN; textMetricsN += fb.textMetricsN; textBlobN += fb.textBlobN;
      t = perfNow(); drawHud(surf->getCanvas()); hudMs += perfMs(t, perfNow());
    }
    const int n = benchFrames;
    const double total = stepMs + renderMs + hudMs;
    const double totalAvg = total / n;   // per-frame pipeline cost (ms)
    const double renderCore = renderMs - svgMs - glowMs;   // cells/geom beyond glow+svg
    auto row = [totalAvg](const char* what, double ms, const char* note) {
      fprintf(stderr, "  %-34s %8.3f ms  %5.1f%%   %s\n", what, ms,
              totalAvg > 0 ? 100.0 * ms / totalAvg : 0.0, note);
    };
    fprintf(stderr,
            "== frame budget @ %dx%d (%d-frame avg; offscreen — present not "
            "included) ==\n",
            benchW, benchH, n);
    row("Model step (game->step)", stepMs / n, "");
    row("Render  (renderTo: cells+items)", renderMs / n, "");
    row("  ├─ geometry/cells & other", renderCore / n, "(render minus svg+glow+text)");
    row("  ├─ item contour glow (blurred sprite)", glowMs / n, "per item");
    row("  ├─ chess-piece SVG (per-instance)", svgMs / n, "");
    row("  └─ badge text (vector font)", textMs / n, "metrics+blob+draw");
    fprintf(stderr,
            "       └─ text breakdown/frame:\n"
            "          metrics (width+height) %8.3f ms  (%.0f calls)\n"
            "          blob build (CoreText)  %8.3f ms  (%.0f blobs)\n"
            "          drawTextBlob           %8.3f ms\n",
            textMetricsMs / n, (double)textMetricsN / n,
            textBlobMs / n, (double)textBlobN / n,
            textDrawMs / n);
    row("HUD", hudMs / n, "");
    fprintf(stderr,
            "  ------------------------------------------------\n"
            "  frame (offscreen render pipeline) ≈ %8.3f ms  →  ~%.0f FPS\n",
            total / n, 1000.0 / (total / n));
    fprintf(stderr, "  SVG pieces drawn/frame ≈ %.1f   badges drawn/frame ≈ %.1f\n",
            (double)svgN / n, (double)textN / n);
    return 0;
  }

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  initFont();

  s_window = SDL_CreateWindow("fractory blend2d", 800, 600, SDL_WINDOW_RESIZABLE);
  if (!s_window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }
  int wx = 0, wy = 0, ww = 800, wh = 600;
  if (loadWindowState(wx, wy, ww, wh)) {
    SDL_SetWindowPosition(s_window, wx, wy);
    SDL_SetWindowSize(s_window, ww, wh);
  }
  SDL_GetWindowSize(s_window, &ww, &wh);
  winW = ww; winH = wh;

  s_vsyncOn = cfg.vsync;
#if !defined(__EMSCRIPTEN__)
  s_gpuActive = false;
  if (useGpu)
    s_gpuActive = gpuSetup(s_window);   // init Metal Ganesh; false => CPU fallback
  if (s_gpuActive) {
    int lw = 0, lh = 0, pxl = 0, pyl = 0;
    SDL_GetWindowSize(s_window, &lw, &lh);
    SDL_GetWindowSizeInPixels(s_window, &pxl, &pyl);
    fprintf(stderr, "[gpu] metal drawable=%dx%d (%dx%d px) vsync=%s\n",
            lw, lh, pxl, pyl, s_vsyncOn ? "on" : "off");
  } else
#endif
  {
    // CPU direct-to-screen path: Skia rasterizes straight into the window's
    // backing surface (RasterDirect, no GPU texture). VSync is a no-op here.
    SDL_Surface *wSurface = SDL_GetWindowSurface(s_window);
    if (!wSurface) {
      fprintf(stderr, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
      return 1;
    }
    if (SDL_BYTESPERPIXEL(wSurface->format) != 4) {
      fprintf(stderr, "Unsupported window surface pixel depth: %d bpp\n",
              SDL_BYTESPERPIXEL(wSurface->format));
      return 1;
    }
    {
      // Window-surface diagnostic (CPU path): surface == logical (HDPI off).
      int lw = 0, lh = 0, pxl = 0, pyl = 0;
      SDL_GetWindowSize(s_window, &lw, &lh);
      SDL_GetWindowSizeInPixels(s_window, &pxl, &pyl);
      fprintf(stderr, "[hidpi] logical=%dx%d pixels=%dx%d surface=%dx%d scale=%.2f\n",
              lw, lh, pxl, pyl, wSurface->w, wSurface->h,
              (lh > 0) ? (float)wSurface->h / (float)lh : 1.0f);
    }
  }

  s_game = std::make_unique<Game>(cfg);
#if !defined(__EMSCRIPTEN__)
  s_game->view().loadChessAssets();
  s_game->view().loadUiAssets();
#endif
  s_perfFreq = SDL_GetPerformanceFrequency();
  s_lastFrameMs = SDL_GetTicks();
  s_lastFpsTime = s_lastFrameMs;

  bool quit = false;
  while (!quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT)
        quit = true;
      else if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_MOVED)
        saveWindowState();
      else if (e.type == SDL_EVENT_MOUSE_MOTION)
        s_game->update((int)e.motion.x, (int)e.motion.y, winW, winH);
      else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        int mod = (SDL_GetModState() & SDL_KMOD_SHIFT) ? 1 : 0;
        s_game->mouseDown(e.button.button, mod, (int)e.button.x, (int)e.button.y, winW, winH);
      }
      else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        s_game->mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
      else if (e.type == SDL_EVENT_MOUSE_WHEEL)
        s_game->mouseWheel((float)e.wheel.x, (float)e.wheel.y, (int)e.wheel.mouse_x, (int)e.wheel.mouse_y, winW, winH);
      else if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_F3) {
          s_profilerOn = !s_profilerOn;   // live on-screen frame profiler
        } else {
          Key k = Key::UNKNOWN;
          if (e.key.key == SDLK_F1) k = Key::F1;
          bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
          s_game->keyDown(k, shift, winW, winH);
        }
      }
    }

    // Fetch current window size each frame (drives render + present size).
    SDL_GetWindowSize(s_window, &winW, &winH);

    uint64_t now = SDL_GetTicks();
    uint64_t st0 = perfNow();
    s_game->step((int)(now - s_lastFrameMs));
    s_msStep = ema(s_msStep, (float)perfMs(st0, perfNow()));
    s_lastFrameMs = now;

    uint64_t f0 = perfNow();

#if !defined(__EMSCRIPTEN__)
    if (s_gpuActive) {
      uint64_t f1 = perfNow();
      bool ok = gpuFrame();            // render to the Metal drawable + present
      uint64_t f3 = perfNow();
      s_msFrame   = ema(s_msFrame,   (float)perfMs(f0, f3));
      s_msCopy    = ema(s_msCopy,    (float)perfMs(f0, f1));
      s_msRender  = ema(s_msRender,  (float)perfMs(f1, f3));  // includes present wait
      s_msPresent = ema(s_msPresent, (float)perfMs(f3, f3));
    } else
#endif
    {
      // Re-fetch the window surface EVERY frame. SDL recreates the surface when
      // the window moves to a display with a different backing scale / format
      // (e.g. 1x vs 2x Retina), so a pointer cached at startup goes stale — using
      // it later can HANG SDL_UpdateWindowSurface (this was the monitor-change
      // freeze). A fresh surface means valid pixels/pitch on every frame.
      SDL_Surface *wSurf = SDL_GetWindowSurface(s_window);
      if (wSurf && SDL_BYTESPERPIXEL(wSurf->format) == 4) {
        if (SDL_MUSTLOCK(wSurf))
          SDL_LockSurface(wSurf);
        // Wrap the current window surface pixels directly and rasterize in place.
        SkImageInfo info = SkImageInfo::Make(wSurf->w, wSurf->h,
                                             kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, wSurf->pixels, (size_t)wSurf->pitch);
        uint64_t f1 = surf ? perfNow() : f0;
        if (surf) {
          SkCanvas *canvas = surf->getCanvas();
          s_game->renderTo(canvas, wSurf->w, wSurf->h);
          drawHud(canvas);
        }
        if (SDL_MUSTLOCK(wSurf))
          SDL_UnlockSurface(wSurf);
        uint64_t f2 = perfNow();

        SDL_UpdateWindowSurface(s_window);   // blit our surface to the real window
        uint64_t f3 = perfNow();

        s_msFrame   = ema(s_msFrame,   (float)perfMs(f0, f3));
        s_msCopy    = ema(s_msCopy,    (float)perfMs(f0, f1));
        s_msRender  = ema(s_msRender,  (float)perfMs(f1, f2));
        s_msPresent = ema(s_msPresent, (float)perfMs(f2, f3));
      } else {
        // Surface temporarily unavailable mid monitor-switch: skip present this
        // frame but keep looping (don't peg the HUD / spin on a stale surface).
        s_msFrame = ema(s_msFrame, (float)perfMs(f0, perfNow()));
      }
    }
    // Periodic profile beacon: where does the frame time actually go?
    if (now - s_lastProfileTick >= 1000) {
      s_lastProfileTick = now;
      Game::StepProfile sp = s_game->stepProfile();
      fprintf(stderr,
              "[prof] step=%6.1fms (tick=%6.1f x%d | parts=%5.1f | eff=%5.1f) "
              "copy=%5.1f run=%5.1f hwPresent=%5.1f total=%6.1fms fps=%.0f\n",
              s_msStep, sp.tickMs, sp.ticks, sp.partsMs, sp.effectsMs,
              s_msCopy, s_msRender, s_msPresent, s_msFrame, s_fps);
      fprintf(stderr,
              "      tick: sweep=%6.1fms nodes=%-8d items=%-8d events=%-6d "
              "spills=%-6d (spillMs=%6.1f)\n",
              sp.tick.sweepMs, sp.tick.nodes, sp.tick.items, sp.tick.events,
              sp.tick.spills, sp.tick.spillMs);
    }
  }

  saveWindowState();
#if !defined(__EMSCRIPTEN__)
  if (s_gpuActive)
    frct_gpu_shutdown();
#endif
  SDL_DestroyWindow(s_window);
  SDL_Quit();
  return 0;
}

#endif