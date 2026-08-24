#include "font.hpp"
#include "game/config.hpp"
#include "game/game.hpp"
#include <SDL2/SDL.h>
#include <blend2d/blend2d.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

static std::unique_ptr<Game> s_game;
static int winW = 800, winH = 600;
static uint32_t s_lastFrameMs = 0;
static int s_frameCount = 0;
static uint32_t s_lastFpsTime = 0;
static float s_fps = 0.0f;

// Uniform RGBA8888 byte buffer shared by native upload and web putImageData.
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

// Overlay the HUD onto the game image by re-opening a context on it.
static void drawHud() {
  updateFps();
  int disp = (int)(s_fps + 0.5f);
  BLImage &img = const_cast<BLImage &>(s_game->image());
  BLContext ctx(img);
  ctx.set_comp_op(BL_COMP_OP_SRC_OVER);
  char t[7][64];
  snprintf(t[0], 64, "FPS: %d", disp);
  snprintf(t[1], 64, "Zoom: x%.1f", (double)s_game->zoomFactor());
  snprintf(t[2], 64, "Cells: %d", s_game->lastCellDrawCount());
  snprintf(t[3], 64, "Depth: %d", s_game->anchorDepth());
  snprintf(t[4], 64, "Gen: %.0fus", (double)(s_game->lastGenMs() * 1000.0f));
  snprintf(t[5], 64, "Draw: %.0fus", (double)(s_game->lastDrawMs() * 1000.0f));
  snprintf(t[6], 64, "v%s", FRACTORY_VERSION);
  for (int i = 0; i < 7; i++)
    drawText(ctx, winW - textWidth(t[i], 1) - 4, 4 + i * 10, t[i], 1);
  ctx.end();
}

// Copy a Blend2D PRGB32 image (B,G,R,A bytes on LE, native endian) into a packed
// RGBA8888 buffer (R,G,B,A) suitable for SDL textures and canvas ImageData.
static void copyImageToRGBA() {
  const BLImage &img = s_game->image();
  BLImageData data;
  if (img.get_data(&data) != BL_SUCCESS)
    return;
  int w = data.size.w, h = data.size.h;
  if (w != s_rgbaW || h != s_rgbaH) {
    s_rgbaW = w;
    s_rgbaH = h;
    s_rgba.resize((size_t)w * h * 4);
  }
  const uint8_t *src = (const uint8_t *)data.pixel_data;
  const size_t srcStride = (size_t)data.stride;
  uint8_t *dst = s_rgba.data();
  for (int y = 0; y < h; y++) {
    const uint8_t *row = src + y * srcStride;
    uint8_t *out = dst + (size_t)y * w * 4;
    for (int x = 0; x < w; x++) {
      uint8_t b = row[x * 4 + 0];
      uint8_t g = row[x * 4 + 1];
      uint8_t r = row[x * 4 + 2];
      out[x * 4 + 0] = r;
      out[x * 4 + 1] = g;
      out[x * 4 + 2] = b;
      out[x * 4 + 3] = 0xFF;
    }
  }
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
  }

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      emscripten_cancel_main_loop();
    // mouse
    if (e.type == SDL_MOUSEMOTION)
      s_game->update((int)e.motion.x, (int)e.motion.y, winW, winH);
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
      int mod = (e.key.keysym.mod & KMOD_SHIFT) ? 1 : 0;
      s_game->mouseDown(e.button.button, mod, (int)e.button.x, (int)e.button.y, winW, winH);
    } else if (e.type == SDL_MOUSEBUTTONUP)
      s_game->mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
    else if (e.type == SDL_MOUSEWHEEL)
      s_game->mouseWheel((float)e.wheel.x, (float)e.wheel.y, (int)e.wheel.mouseX, (int)e.wheel.mouseY, winW, winH);
    else if (e.type == SDL_KEYDOWN) {
      Key k = Key::UNKNOWN;
      if (e.key.keysym.sym == SDLK_F1) k = Key::F1;
      bool shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
      s_game->keyDown(k, shift, winW, winH);
    }
  }

  uint32_t now = SDL_GetTicks();
  s_game->step((int)(now - s_lastFrameMs));
  s_lastFrameMs = now;

  s_game->render(winW, winH);
  drawHud();
  copyImageToRGBA();
  jsPresent(s_rgbaW, s_rgbaH, s_rgba.data());
}

int main() {
  bl_runtime_init();
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  initFont();

  s_window = SDL_CreateWindow("fractory blend2d", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 800, 600,
                              SDL_WINDOW_RESIZABLE);
  if (!s_window)
    return 1;

  int w, h;
  SDL_GetWindowSize(s_window, &w, &h);
  winW = w; winH = h;
  emscripten_set_canvas_element_size("#canvas", winW, winH);

  EM_ASM({
    Module._canvasCtx = document.querySelector('#canvas').getContext('2d');
  });

  s_game = std::make_unique<Game>(Config{});
  s_lastFrameMs = SDL_GetTicks();
  s_lastFpsTime = s_lastFrameMs;

  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}

#else // ---- Native SDL2 ----

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
  Config cfg;
  for (int a = 1; a < argc; a++) {
    if (std::string(argv[a]) == "--empty-grid") {
      cfg.emptyGrid = true;
      if (a + 1 < argc && std::string(argv[a + 1]).find_first_not_of("0123456789") == std::string::npos)
        cfg.initialGridSize = std::atoi(argv[++a]);
    }
  }

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  initFont();

  s_window = SDL_CreateWindow("fractory blend2d", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 800, 600,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
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

  SDL_Renderer *renderer = SDL_CreateRenderer(s_window, -1,
      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(s_window, -1, 0);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Texture *tex = nullptr;

  s_game = std::make_unique<Game>(cfg);
  s_lastFrameMs = SDL_GetTicks();
  s_lastFpsTime = s_lastFrameMs;

  bool quit = false;
  while (!quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        quit = true;
      else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED)
        saveWindowState();
      else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_MOVED)
        saveWindowState();
      else if (e.type == SDL_MOUSEMOTION)
        s_game->update((int)e.motion.x, (int)e.motion.y, winW, winH);
      else if (e.type == SDL_MOUSEBUTTONDOWN) {
        int mod = (e.key.keysym.mod & KMOD_SHIFT) ? 1 : 0;
        s_game->mouseDown(e.button.button, mod, (int)e.button.x, (int)e.button.y, winW, winH);
      }
      else if (e.type == SDL_MOUSEBUTTONUP)
        s_game->mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
      else if (e.type == SDL_MOUSEWHEEL)
        s_game->mouseWheel((float)e.wheel.x, (float)e.wheel.y, (int)e.wheel.mouseX, (int)e.wheel.mouseY, winW, winH);
      else if (e.type == SDL_KEYDOWN) {
        Key k = Key::UNKNOWN;
        if (e.key.keysym.sym == SDLK_F1) k = Key::F1;
        bool shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
        s_game->keyDown(k, shift, winW, winH);
      }
    }

    // Recreate texture on resize
    if (tex && (winW != s_rgbaW || winH != s_rgbaH))
      { SDL_DestroyTexture(tex); tex = nullptr; }

    SDL_GetWindowSize(s_window, &winW, &winH);

    uint32_t now = SDL_GetTicks();
    s_game->step((int)(now - s_lastFrameMs));
    s_lastFrameMs = now;

    s_game->render(winW, winH);
    drawHud();
    copyImageToRGBA();

    if (!tex) {
      tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                              SDL_TEXTUREACCESS_STREAMING, winW, winH);
    }
    SDL_UpdateTexture(tex, nullptr, s_rgba.data(), s_rgbaW * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_RenderPresent(renderer);
  }

  saveWindowState();
  if (tex) SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(s_window);
  SDL_Quit();
  return 0;
}

#endif