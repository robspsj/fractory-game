#include "font.hpp"
#include "game/config.hpp"
#include "game/game.hpp"
#include "test_runner.hpp"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
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
static float s_msRender = 0.0f;  // Skia render + HUD
static float s_msCopy = 0.0f;    // surface binding / buffer wrap
static float s_msPresent = 0.0f; // present (SDL surface blit / canvas putImageData)
static uint64_t s_perfFreq = 0;
static bool s_vsyncOn = false;

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
bool frct_gpu_init(void*);
bool frct_gpu_frame(void (*render)(void*, SkCanvas*, int, int), void* ctx);
void frct_gpu_shutdown(void);
}
static bool gpuSetup(SDL_Window* win) { return (s_gpuActive = frct_gpu_init(win)); }
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

// Overlay the HUD. The caller's canvas carries the game's NDC matrix; we reset
// to an identity (pixel) matrix before drawing the monospace 8x8 font.
static void drawHud(SkCanvas *canvas) {
  updateFps();
  canvas->resetMatrix();
  int disp = (int)(s_fps + 0.5f);
  char t[11][72];
  snprintf(t[0],  72, "FPS: %d  %c", disp, s_vsyncOn ? 'V' : 'v');
  snprintf(t[1],  72, "Frame: %.2fms", (double)s_msFrame);
  snprintf(t[2],  72, "  Render: %.2fms", (double)s_msRender);
  snprintf(t[3],  72, "  Copy:   %.2fms", (double)s_msCopy);
  snprintf(t[4],  72, "  Present:%.2fms", (double)s_msPresent);
  snprintf(t[5],  72, "Cells: %d  Depth: %d", s_game->lastCellDrawCount(), s_game->anchorDepth());
  snprintf(t[6],  72, "Zoom: x%.2f", (double)s_game->zoomFactor());
  snprintf(t[7],  72, "res: %dx%d", winW, winH);
  snprintf(t[8],  72, "v%s", FRACTORY_VERSION);
  {
    std::size_t mem = s_game->allocatedMemory();
    const char* units[] = {"B", "KB", "MB", "GB"};
    int u = 0;
    double v = (double)mem;
    while (v >= 1024.0 && u < 3) { v /= 1024.0; u++; }
    snprintf(t[9], 72, "Mem: %.2f %s", v, units[u]);
  }
  t[10][0] = '\0';
  for (int i = 0; i < 10; i++)
    drawText(canvas, winW - textWidth(t[i], 1) - 4, 4 + i * 10, t[i], 1);
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
  std::string headlessPath;
  int headW = 800, headH = 600;
  // --bench [W H] [frames]: render N frames offscreen at W x H and report the
  // frame-budget breakdown (step / render / SVG / glow / HUD / dither). No
  // window, no SDL present; present (SDL UpdateWindowSurface) is not included.
  bool benchMode = false;
  int benchW = 3840, benchH = 2049, benchFrames = 5;
  // --single <out.ppm> <elementIdx> <size>: render ONE icon (piece+glow) centred
  // on a <size>x<size> surface, to measure piece-vs-glow alignment.
  std::string singlePath;
  int singleIdx = 0, singleSize = 600;
  bool useGpu = true;   // native default; --cpu forces the CPU present path
  for (int a = 1; a < argc; a++) {
    if (std::string(argv[a]) == "--empty-grid") {
      cfg.emptyGrid = true;
      if (a + 1 < argc && std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos)
        cfg.initialGridSize = std::atoi(argv[++a]);
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
    }
  }

  if (!headlessPath.empty()) {
    Game g(cfg);
#if !defined(__EMSCRIPTEN__)
    g.view().loadChessAssets();
#endif
    std::vector<uint8_t> buf((size_t)headW * headH * 4);
    SkImageInfo info = SkImageInfo::Make(headW, headH, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, buf.data(), (size_t)headW * 4);
    if (!surf)
      return 1;
    g.renderTo(surf->getCanvas(), headW, headH);
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
#endif
    std::vector<uint8_t> buf((size_t)benchW * benchH * 4);
    SkImageInfo info = SkImageInfo::Make(benchW, benchH, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType);
    sk_sp<SkSurface> surf = SkSurfaces::WrapPixels(info, buf.data(), (size_t)benchW * 4);
    if (!surf)
      return 1;
    double stepMs = 0, renderMs = 0, hudMs = 0, svgMs = 0, glowMs = 0;
    long svgN = 0;
    for (int f = 0; f < benchFrames; f++) {
      s_game->view().resetFramebench();
      uint64_t t;
      t = perfNow(); s_game->step(16); stepMs += perfMs(t, perfNow());
      t = perfNow(); s_game->renderTo(surf->getCanvas(), benchW, benchH); renderMs += perfMs(t, perfNow());
      const Framebench& fb = s_game->view().framebench();
      svgMs += fb.svgMs; glowMs += fb.glowMs; svgN += fb.svgN;
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
    row("  ├─ geometry/cells & other", renderCore / n, "(render minus svg+glow)");
    row("  ├─ item contour glow (blurred sprite)", glowMs / n, "per item");
    row("  └─ chess-piece SVG (per-instance)", svgMs / n, "");
    row("HUD", hudMs / n, "");
    fprintf(stderr,
            "  ------------------------------------------------\n"
            "  frame (offscreen render pipeline) ≈ %8.3f ms  →  ~%.0f FPS\n",
            total / n, 1000.0 / (total / n));
    fprintf(stderr, "  SVG pieces drawn/frame ≈ %.1f\n", (double)svgN / n);
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
        Key k = Key::UNKNOWN;
        if (e.key.key == SDLK_F1) k = Key::F1;
        bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
        s_game->keyDown(k, shift, winW, winH);
      }
    }

    // Fetch current window size each frame (drives render + present size).
    SDL_GetWindowSize(s_window, &winW, &winH);

    uint64_t now = SDL_GetTicks();
    s_game->step((int)(now - s_lastFrameMs));
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