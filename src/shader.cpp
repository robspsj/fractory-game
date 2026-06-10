#include "shader.hpp"
#include <cstdio>

const char *vertSrc = R"(
attribute vec2 aPos;
attribute vec3 aColor;
uniform vec2 uPos;
uniform float uAspect;
varying vec3 vColor;
void main() {
    vec2 pos = aPos;
    pos.y *= uAspect;
    gl_Position = vec4(pos + uPos, 0.0, 1.0);
    vColor = aColor;
}
)";

#ifdef __EMSCRIPTEN__
const char *fragSrc = R"(
precision mediump float;
varying vec3 vColor;
void main() { gl_FragColor = vec4(vColor, 1.0); }
)";
#else
const char *fragSrc = R"(
varying vec3 vColor;
void main() { gl_FragColor = vec4(vColor, 1.0); }
)";
#endif

int compile(GLenum type, const char *src) {
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
