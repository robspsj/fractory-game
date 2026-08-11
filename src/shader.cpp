#include "shader.hpp"
#include <cstdio>

const char *vertSrc = R"(
attribute vec2 aPos;
attribute vec3 aColor;
attribute vec2 aTexCoord;
varying vec3 vColor;
varying vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
)";

#ifdef __EMSCRIPTEN__
const char *fragSrc = R"(
precision mediump float;
varying vec3 vColor;
varying vec2 vTexCoord;
uniform sampler2D uTex;
void main() {
    vec4 texColor = texture2D(uTex, vTexCoord);
    gl_FragColor = texColor * vec4(vColor, 1.0);
}
)";
#else
const char *fragSrc = R"(
varying vec3 vColor;
varying vec2 vTexCoord;
uniform sampler2D uTex;
void main() {
    vec4 texColor = texture2D(uTex, vTexCoord);
    gl_FragColor = texColor * vec4(vColor, 1.0);
}
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
