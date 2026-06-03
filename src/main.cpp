#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <SDL3/SDL_opengles2.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#include <OpenGL/gl.h>
#endif

static const char *vertSrc = R"(
attribute vec2 aPos;
uniform vec2 uPos;
void main() { gl_Position = vec4(aPos + uPos, 0.0, 1.0); }
)";

#ifdef __EMSCRIPTEN__
static const char *fragSrc = R"(
precision mediump float;
void main() { gl_FragColor = vec4(1.0, 0.5, 0.2, 1.0); }
)";
#else
static const char *fragSrc = R"(
void main() { gl_FragColor = vec4(1.0, 0.5, 0.2, 1.0); }
)";
#endif

static GLuint prog, uPos;
static float mouseX = 0.0f, mouseY = 0.0f;
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

static int compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), 0, log);
        fprintf(stderr, "shader error: %s\n", log);
    }
    return s;
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

static void updateMouse(int px, int py) {
    mouseX = (2.0f * px / winW) - 1.0f;
    mouseY = 1.0f - (2.0f * py / winH);
}

// -- bitmap font overlay (8x8 pixel monospace) --
#define FONT_W 8
#define FONT_H 8
#define NUM_GLYPH 15
// 15 glyphs: space, 0-9, :, F, P, S  (left to right in texture)
static const unsigned char fontBits[NUM_GLYPH * FONT_H] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // space
    0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00, // 0
    0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00, // 1
    0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00, // 2
    0x7E,0x06,0x0C,0x1C,0x06,0x66,0x3C,0x00, // 3
    0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00, // 4
    0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00, // 5
    0x3C,0x60,0x7C,0x66,0x66,0x66,0x3C,0x00, // 6
    0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00, // 7
    0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00, // 8
    0x3C,0x66,0x66,0x3E,0x06,0x06,0x3C,0x00, // 9
    0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00, // :
    0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00, // F
    0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00, // P
    0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00, // S
};

static const char *textVertSrc = R"(
attribute vec2 aPos;
attribute vec2 aTex;
uniform vec2 uScreen;
varying vec2 vTex;
void main() {
    gl_Position = vec4(2.0*aPos.x/uScreen.x-1.0, 1.0-2.0*aPos.y/uScreen.y, 0.0, 1.0);
    vTex = aTex;
}
)";

#ifdef __EMSCRIPTEN__
static const char *textFragSrc = R"(
precision mediump float;
uniform sampler2D uTex;
uniform vec3 uColor;
varying vec2 vTex;
void main() {
    float a = texture2D(uTex, vTex).r;
    gl_FragColor = vec4(uColor, a);
}
)";
#else
static const char *textFragSrc = R"(
uniform sampler2D uTex;
uniform vec3 uColor;
varying vec2 vTex;
void main() {
    float a = texture2D(uTex, vTex).r;
    gl_FragColor = vec4(uColor, a);
}
)";
#endif

static GLuint textProg, fontTex, textVbo;
static GLint textUTex, textUColor, textUScreen;

static void initFont() {
    unsigned char tex[NUM_GLYPH * FONT_W * FONT_H] = {0};
    for (int g = 0; g < NUM_GLYPH; g++) {
        for (int r = 0; r < FONT_H; r++) {
            unsigned char bits = fontBits[g * FONT_H + r];
            for (int c = 0; c < FONT_W; c++)
                tex[g * FONT_W + c + r * (NUM_GLYPH * FONT_W)] =
                    (bits & (0x80 >> c)) ? 255 : 0;
        }
    }
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, NUM_GLYPH * FONT_W, FONT_H, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    textProg = glCreateProgram();
    glBindAttribLocation(textProg, 1, "aPos");
    glBindAttribLocation(textProg, 2, "aTex");
    glAttachShader(textProg, compile(GL_VERTEX_SHADER, textVertSrc));
    glAttachShader(textProg, compile(GL_FRAGMENT_SHADER, textFragSrc));
    glLinkProgram(textProg);
    glUseProgram(textProg);
    textUTex = glGetUniformLocation(textProg, "uTex");
    textUColor = glGetUniformLocation(textProg, "uColor");
    textUScreen = glGetUniformLocation(textProg, "uScreen");

    glUniform1i(textUTex, 0);
    glUniform3f(textUColor, 0.0f, 1.0f, 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);

    glGenBuffers(1, &textVbo);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(prog);
}

static void drawText(float x, float y, const char *text, float scale, float *vertsOut, int *outLen) {
    int len = (int)strlen(text);
    if (len > 32) len = 32;
    *outLen = len;

    float charW = FONT_W * scale, charH = FONT_H * scale;
    for (int i = 0; i < len; i++) {
        int g;
        char c = text[i];
        if (c == ' ')      g = 0;
        else if (c >= '0' && c <= '9') g = 1 + (c - '0');
        else if (c == ':') g = 11;
        else if (c == 'F') g = 12;
        else if (c == 'P') g = 13;
        else if (c == 'S') g = 14;
        else continue;

        float x0 = x + i * charW, y0 = y;
        float x1 = x0 + charW, y1 = y0 + charH;
        float tx0 = (float)g / NUM_GLYPH, tx1 = (float)(g + 1) / NUM_GLYPH;

        float *v = vertsOut + i * 24;
        v[0]=x0; v[1]=y1; v[2]=tx0; v[3]=1.0f;
        v[4]=x1; v[5]=y1; v[6]=tx1; v[7]=1.0f;
        v[8]=x0; v[9]=y0; v[10]=tx0; v[11]=0.0f;
        v[12]=x1; v[13]=y1; v[14]=tx1; v[15]=1.0f;
        v[16]=x0; v[17]=y0; v[18]=tx0; v[19]=0.0f;
        v[20]=x1; v[21]=y0; v[22]=tx1; v[23]=0.0f;
    }
}

static void drawTextGl(const float *verts, int len) {
    glUseProgram(textProg);
    glUniform2f(textUScreen, (float)winW, (float)winH);

    glBindBuffer(GL_ARRAY_BUFFER, textVbo);
    glBufferData(GL_ARRAY_BUFFER, len * 24 * sizeof(float), verts, GL_STREAM_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (int)sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * (int)sizeof(float), (void*)(2LL * sizeof(float)));
    glEnableVertexAttribArray(2);

    glDrawArrays(GL_TRIANGLES, 0, 6 * len);

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glUseProgram(prog);
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
        if (e.type == SDL_EVENT_MOUSE_MOTION) updateMouse(e.motion.x, e.motion.y);
    }

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2f(uPos, mouseX, mouseY);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

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
    if (len) drawTextGl(verts, len);

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

    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vertSrc));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fragSrc));
    glLinkProgram(prog);
    glUseProgram(prog);

    GLfloat verts[] = {
        -0.15f, -0.15f,   0.15f, -0.15f,
        -0.15f,  0.15f,   0.15f,  0.15f,
    };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint aPos = glGetAttribLocation(prog, "aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aPos);

    uPos = glGetUniformLocation(prog, "uPos");
    initFont();

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    resizeCanvas(w, h);
    resize(w, h);

    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
#else
int main() {
    SDL_Init(SDL_INIT_VIDEO);
    lastFpsTime = SDL_GetTicks();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window *win = SDL_CreateWindow("fractory", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(0);

    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vertSrc));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fragSrc));
    glLinkProgram(prog);
    glUseProgram(prog);

    GLfloat verts[] = {
        -0.15f, -0.15f,   0.15f, -0.15f,
        -0.15f,  0.15f,   0.15f,  0.15f,
    };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint aPos = glGetAttribLocation(prog, "aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aPos);

    uPos = glGetUniformLocation(prog, "uPos");
    initFont();

    int vw, vh;
    SDL_GetWindowSize(win, &vw, &vh);
    resize(vw, vh);

    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) quit = true;
            if (e.type == SDL_EVENT_WINDOW_RESIZED) resize(e.window.data1, e.window.data2);
            if (e.type == SDL_EVENT_MOUSE_MOTION) updateMouse(e.motion.x, e.motion.y);
        }
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform2f(uPos, mouseX, mouseY);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

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
        if (len) drawTextGl(verts, len);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
#endif
