#include "shader.hpp"
#include <cstdio>

const char *vertSrc = R"(
attribute vec2 aPos;
attribute vec3 aColor;
attribute vec2 aTileMin;
attribute vec2 aTileMax;
attribute vec2 aTileUV;
varying vec3 vColor;
varying vec2 vTileMin;
varying vec2 vTileMax;
varying vec2 vTileUV;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vTileMin = aTileMin;
    vTileMax = aTileMax;
    vTileUV = aTileUV;
}
)";

#ifdef __EMSCRIPTEN__
const char *fragSrc = R"(
precision mediump float;
varying vec3 vColor;
varying vec2 vTileMin;
varying vec2 vTileMax;
varying vec2 vTileUV;
uniform sampler2D uTex;
uniform vec2 uAtlasSize;

void main() {
    vec2 tilePx = (vTileMax - vTileMin) * uAtlasSize;
    vec2 tUV = clamp(vTileUV, 0.0, 1.0);
    vec2 texel = tUV * tilePx - 0.5;
    vec2 nearest = floor(texel) + 0.5;
    vec2 tileMinPx = vTileMin * uAtlasSize + 0.5;
    vec2 tileMaxPx = vTileMax * uAtlasSize - 0.5;
    nearest = clamp(nearest, tileMinPx, tileMaxPx);
    vec4 texColor = texture2D(uTex, nearest / uAtlasSize);
    gl_FragColor = texColor * vec4(vColor, 1.0);
}
)";
#else
const char *fragSrc = R"(
varying vec3 vColor;
varying vec2 vTileMin;
varying vec2 vTileMax;
varying vec2 vTileUV;
uniform sampler2D uTex;
uniform vec2 uAtlasSize;

void main() {
    vec2 tilePx = (vTileMax - vTileMin) * uAtlasSize;
    vec2 tUV = clamp(vTileUV, 0.0, 1.0);
    vec2 texel = tUV * tilePx - 0.5;
    vec2 nearest = floor(texel) + 0.5;
    vec2 tileMinPx = vTileMin * uAtlasSize + 0.5;
    vec2 tileMaxPx = vTileMax * uAtlasSize - 0.5;
    nearest = clamp(nearest, tileMinPx, tileMaxPx);
    vec4 texColor = texture2D(uTex, nearest / uAtlasSize);
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
