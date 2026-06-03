#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <SDL_opengles2.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#include <OpenGL/gl.h>
#endif

static const char *vertSrc = R"(
attribute vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
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

static GLuint prog;

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
    glViewport(0, 0, w, h);
}

#ifdef __EMSCRIPTEN__
static void resizeCanvas(int w, int h) {
    emscripten_set_canvas_element_size("#canvas", w, h);
}
#endif

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
        if (e.type == SDL_QUIT) emscripten_cancel_main_loop();
    }

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    SDL_GL_SwapWindow(window);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow("fractory", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) return 1;

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) return 1;

    prog = glCreateProgram();
    GLuint vs = compile(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragSrc);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) return 1;
    glUseProgram(prog);

    GLfloat verts[] = { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint aPos = glGetAttribLocation(prog, "aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aPos);

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
    SDL_Window *win = SDL_CreateWindow("fractory", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);

    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vertSrc));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fragSrc));
    glLinkProgram(prog);
    glUseProgram(prog);

    GLfloat verts[] = { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint aPos = glGetAttribLocation(prog, "aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aPos);

    int vw, vh;
    SDL_GetWindowSize(win, &vw, &vh);
    resize(vw, vh);

    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto done;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                resize(e.window.data1, e.window.data2);
            }
        }
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        SDL_GL_SwapWindow(win);
    }

done:
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
#endif
