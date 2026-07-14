#include "font.hpp"
#include "game/config.hpp"
#include "game/game.hpp"
#include "print_state.hpp"
#include "test_runner.hpp"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

static std::unique_ptr<Game> s_game;
static int winW = 800, winH = 600;
static Uint64 lastFpsTime = 0;
static int frameCount = 0;
static float fps = 0.0f;
static int lastFpsDisplay = -1;
static char fpsText[32] = "";
static char zoomText[32] = "";
static char vertText[32] = "";

static void updateFps() {
  frameCount++;
  Uint64 now = SDL_GetTicks();
  if (now - lastFpsTime >= 200) {
    fps = frameCount * 1000.0f / (now - lastFpsTime);
    frameCount = 0;
    lastFpsTime = now;
  }
}

static void resize(int w, int h) {
  winW = w;
  winH = h;
  glViewport(0, 0, w, h);
}

#ifdef __EMSCRIPTEN__
static void resizeCanvas(int w, int h) {
  emscripten_set_canvas_element_size("#canvas", w, h);
}
#endif

static void drawFpsOverlay() {
  updateFps();
  int disp = (int)(fps + 0.5f);
  if (disp != lastFpsDisplay) {
    lastFpsDisplay = disp;
    snprintf(fpsText, sizeof(fpsText), "FPS: %d", disp);
  }
  float zoom = s_game->zoomFactor();
  snprintf(zoomText, sizeof(zoomText), "Zoom: ×%.1f", zoom);
  int verts = s_game->lastVertexCount();
  snprintf(vertText, sizeof(vertText), "V: %d", verts);
  int fpsLen = (int)strlen(fpsText);
  int zoomLen = (int)strlen(zoomText);
  int vertLen = (int)strlen(vertText);
  int x = winW - fpsLen * 16 - 8;
  float vbuf[32 * 24];
  int len;
  drawText((float)x, 8.0f, fpsText, 2, vbuf, &len);
  if (len)
    drawTextGl(vbuf, len, s_game->program(), winW, winH);
  int zx = winW - zoomLen * 16 - 8;
  drawText((float)zx, 28.0f, zoomText, 2, vbuf, &len);
  if (len)
    drawTextGl(vbuf, len, s_game->program(), winW, winH);
  int vx = winW - vertLen * 16 - 8;
  drawText((float)vx, 48.0f, vertText, 2, vbuf, &len);
  if (len)
    drawTextGl(vbuf, len, s_game->program(), winW, winH);
}

#ifdef __EMSCRIPTEN__
static SDL_Window *window;
static void frame() {
  int w, h;
  SDL_GetWindowSize(window, &w, &h);
  if (w > 0 && h > 0) {
    resizeCanvas(w, h);
    resize(w, h);
  }

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT)
      emscripten_cancel_main_loop();
    if (e.type == SDL_EVENT_MOUSE_MOTION)
      s_game->update((int)e.motion.x, (int)e.motion.y, winW, winH);
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
      s_game->mouseDown(e.button.button, (int)e.button.x, (int)e.button.y, winW,
                        winH);
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
      s_game->mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW,
                      winH);
    if (e.type == SDL_EVENT_MOUSE_WHEEL)
      s_game->mouseWheel(e.wheel.x, e.wheel.y, (int)e.wheel.mouse_x,
                         (int)e.wheel.mouse_y, winW, winH);
    if (e.type == SDL_EVENT_KEY_DOWN)
      s_game->keyDown(e.key.key, (SDL_Keymod)e.key.mod, winW, winH);
  }

  glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  s_game->render(w, h);
  drawFpsOverlay();
  SDL_GL_SwapWindow(window);
}

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  lastFpsTime = SDL_GetTicks();
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  window = SDL_CreateWindow("fractory", 800, 600,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window)
    return 1;

  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  if (!ctx)
    return 1;
  SDL_GL_SetSwapInterval(0);

  s_game = std::make_unique<Game>(Config{});
  clearScreen();
  initFont(s_game->program());
  int w, h;
  SDL_GetWindowSize(window, &w, &h);
  resizeCanvas(w, h);
  resize(w, h);
  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
#else

static void saveWindowState(SDL_Window *win) {
  int x, y, w, h;
  SDL_GetWindowPosition(win, &x, &y);
  SDL_GetWindowSize(win, &w, &h);
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
  if (argc >= 3 && std::string(argv[1]) == "--test") {
    int gridLimit = 100;
    for (int a = 3; a + 1 < argc; a++) {
      if (std::string(argv[a]) == "--grid-limit")
        gridLimit = std::atoi(argv[a + 1]);
    }
    bool success = ModelTestRunner::runTest(argv[2], gridLimit);
    return success ? 0 : 1;
  }

  SDL_Init(SDL_INIT_VIDEO);
  lastFpsTime = SDL_GetTicks();

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  SDL_Window *win = SDL_CreateWindow("fractory", 800, 600,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  int wx = 0, wy = 0, ww = 800, wh = 600;
  if (loadWindowState(wx, wy, ww, wh)) {
    SDL_SetWindowPosition(win, wx, wy);
    SDL_SetWindowSize(win, ww, wh);
  }
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  SDL_GL_SetSwapInterval(0);

  s_game = std::make_unique<Game>(Config{});
  clearScreen();
  initFont(s_game->program());

  int vw, vh;
  SDL_GetWindowSize(win, &vw, &vh);
  resize(vw, vh);

  bool quit = false;
  while (!quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT)
        quit = true;
      if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        resize(e.window.data1, e.window.data2);
        saveWindowState(win);
      }
      if (e.type == SDL_EVENT_WINDOW_MOVED)
        saveWindowState(win);
      if (e.type == SDL_EVENT_MOUSE_MOTION)
        s_game->update((int)e.motion.x, (int)e.motion.y, winW, winH);
      if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        s_game->mouseDown(e.button.button, (int)e.button.x, (int)e.button.y,
                          winW, winH);
      }
      if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        s_game->mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW,
                        winH);
      if (e.type == SDL_EVENT_MOUSE_WHEEL)
        s_game->mouseWheel(e.wheel.x, e.wheel.y, (int)e.wheel.mouse_x,
                           (int)e.wheel.mouse_y, winW, winH);
      if (e.type == SDL_EVENT_KEY_DOWN)
        s_game->keyDown(e.key.key, (SDL_Keymod)e.key.mod, winW, winH);
    }
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    s_game->render(winW, winH);
    drawFpsOverlay();
    SDL_GL_SwapWindow(win);
  }

  saveWindowState(win);
  SDL_GL_DestroyContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
#endif
