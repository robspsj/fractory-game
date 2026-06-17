#include "game/game.hpp"
#include "font.hpp"
#include "print_state.hpp"
#include "test_runner.hpp"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

static int winW = 800, winH = 600;
static Uint64 lastFpsTime = 0;
static int frameCount = 0;
static float fps = 0.0f;
static int lastFpsDisplay = -1;
static char fpsText[32] = "";

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
    int x = winW - (int)strlen(fpsText) * 16 - 8;
    float verts[32 * 24];
    int len;
    drawText((float)x, 8.0f, fpsText, 2, verts, &len);
    if (len) drawTextGl(verts, len, gameProgram(), winW, winH);
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
        if (e.type == SDL_EVENT_QUIT) emscripten_cancel_main_loop();
        if (e.type == SDL_EVENT_MOUSE_MOTION) gameUpdate((int)e.motion.x, (int)e.motion.y, winW, winH);
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) gameMouseDown(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) gameMouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
        if (e.type == SDL_EVENT_MOUSE_WHEEL) gameMouseWheel(e.wheel.x, e.wheel.y);
    }

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gameRender(w, h);
    drawFpsOverlay();
    SDL_GL_SwapWindow(window);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    lastFpsTime = SDL_GetTicks();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow("fractory", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) return 1;

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) return 1;
    SDL_GL_SetSwapInterval(0);

    gameInit();
    clearScreen();
    gamePrintState();
    initFont(gameProgram());
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    resizeCanvas(w, h);
    resize(w, h);
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
#else
int main(int argc, char* argv[]) {
    if (argc >= 3 && std::string(argv[1]) == "--test") {
        bool success = ModelTestRunner::runTest(argv[2]);
        return success ? 0 : 1;
    }

    SDL_Init(SDL_INIT_VIDEO);
    lastFpsTime = SDL_GetTicks();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window *win = SDL_CreateWindow("fractory", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(0);

    gameInit();
    clearScreen();
    gamePrintState();
    initFont(gameProgram());

    int vw, vh;
    SDL_GetWindowSize(win, &vw, &vh);
    resize(vw, vh);

    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) quit = true;
            if (e.type == SDL_EVENT_WINDOW_RESIZED) resize(e.window.data1, e.window.data2);
            if (e.type == SDL_EVENT_MOUSE_MOTION) gameUpdate((int)e.motion.x, (int)e.motion.y, winW, winH);
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                gameMouseDown(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) gameMouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW, winH);
            if (e.type == SDL_EVENT_MOUSE_WHEEL) gameMouseWheel(e.wheel.x, e.wheel.y);
        }
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        gameRender(winW, winH);
        drawFpsOverlay();
        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
#endif
