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

vec4 cubic(float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    float w0 = -0.5*t3 + t2 - 0.5*t;
    float w1 =  1.5*t3 - 2.5*t2 + 1.0;
    float w2 = -1.5*t3 + 2.0*t2 + 0.5*t;
    float w3 =  0.5*t3 - 0.5*t2;
    return vec4(w3, w2, w1, w0);
}

void main() {
    vec2 tilePx = (vTileMax - vTileMin) * uAtlasSize;
    vec2 tUV = clamp(vTileUV, 0.0, 1.0);
    vec2 texel = tUV * tilePx - 0.5;
    vec2 f = fract(texel);
    vec2 base = vTileMin * uAtlasSize + floor(texel) + 0.5;
    vec2 invAtlas = 1.0 / uAtlasSize;

    vec4 wx = cubic(f.x);
    vec4 wy = cubic(f.y);

    vec4 color = vec4(0.0);
    for (int y = 0; y < 4; y++) {
        float wyi = (y == 0) ? wy.x : (y == 1) ? wy.y : (y == 2) ? wy.z : wy.w;
        float py = clamp(base.y + float(y - 1), 0.5, uAtlasSize.y - 0.5) * invAtlas.y;
        for (int x = 0; x < 4; x++) {
            float wxi = (x == 0) ? wx.x : (x == 1) ? wx.y : (x == 2) ? wx.z : wx.w;
            float px = clamp(base.x + float(x - 1), 0.5, uAtlasSize.x - 0.5) * invAtlas.x;
            color += texture2D(uTex, vec2(px, py)) * wxi * wyi;
        }
    }
    gl_FragColor = clamp(color, 0.0, 1.0) * vec4(vColor, 1.0);
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

vec4 cubic(float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    float w0 = -0.5*t3 + t2 - 0.5*t;
    float w1 =  1.5*t3 - 2.5*t2 + 1.0;
    float w2 = -1.5*t3 + 2.0*t2 + 0.5*t;
    float w3 =  0.5*t3 - 0.5*t2;
    return vec4(w3, w2, w1, w0);
}

void main() {
    vec2 tilePx = (vTileMax - vTileMin) * uAtlasSize;
    vec2 tUV = clamp(vTileUV, 0.0, 1.0);
    vec2 texel = tUV * tilePx - 0.5;
    vec2 f = fract(texel);
    vec2 base = vTileMin * uAtlasSize + floor(texel) + 0.5;
    vec2 invAtlas = 1.0 / uAtlasSize;

    vec4 wx = cubic(f.x);
    vec4 wy = cubic(f.y);

    vec4 color = vec4(0.0);
    for (int y = 0; y < 4; y++) {
        float wyi = (y == 0) ? wy.x : (y == 1) ? wy.y : (y == 2) ? wy.z : wy.w;
        float py = clamp(base.y + float(y - 1), 0.5, uAtlasSize.y - 0.5) * invAtlas.y;
        for (int x = 0; x < 4; x++) {
            float wxi = (x == 0) ? wx.x : (x == 1) ? wx.y : (x == 2) ? wx.z : wx.w;
            float px = clamp(base.x + float(x - 1), 0.5, uAtlasSize.x - 0.5) * invAtlas.x;
            color += texture2D(uTex, vec2(px, py)) * wxi * wyi;
        }
    }
    gl_FragColor = clamp(color, 0.0, 1.0) * vec4(vColor, 1.0);
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
