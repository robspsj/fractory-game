#include "font.hpp"
#include "shader.hpp"
#include <cstdio>
#include <cstring>

#define FONT_W 8
#define FONT_H 8
#define NUM_GLYPH 15

static const unsigned char fontBits[NUM_GLYPH * FONT_H] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x3C, 0x00, 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00,
    0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00, 0x7E, 0x06, 0x0C, 0x1C,
    0x06, 0x66, 0x3C, 0x00, 0x0C, 0x1C, 0x3C, 0x6C, 0xFE, 0x0C, 0x0C, 0x00,
    0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00, 0x3C, 0x60, 0x7C, 0x66,
    0x66, 0x66, 0x3C, 0x00, 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00,
    0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00, 0x3C, 0x66, 0x66, 0x3E,
    0x06, 0x06, 0x3C, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00, 0x7C, 0x66, 0x66, 0x7C,
    0x60, 0x60, 0x60, 0x00, 0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00,
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
static GLint textUScreen;

void initFont(GLuint mainProg) {
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
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, NUM_GLYPH * FONT_W, FONT_H, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, tex);
  {
    GLenum err = glGetError();
    if (err)
      fprintf(stderr, "GL error after glTexImage2D: 0x%x\n", err);
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  textProg = glCreateProgram();
  glBindAttribLocation(textProg, 0, "aPos");
  glBindAttribLocation(textProg, 1, "aTex");
  glAttachShader(textProg, compile(GL_VERTEX_SHADER, textVertSrc));
  glAttachShader(textProg, compile(GL_FRAGMENT_SHADER, textFragSrc));
  glLinkProgram(textProg);
  {
    GLint linked;
    glGetProgramiv(textProg, GL_LINK_STATUS, &linked);
    if (!linked) {
      char log[512];
      glGetProgramInfoLog(textProg, sizeof(log), 0, log);
      fprintf(stderr, "text program link error: %s\n", log);
    }
    GLenum err = glGetError();
    if (err)
      fprintf(stderr, "GL error after text prog link: 0x%x\n", err);
  }
  glUseProgram(textProg);
  textUScreen = glGetUniformLocation(textProg, "uScreen");
  glUniform1i(glGetUniformLocation(textProg, "uTex"), 0);
  glUniform3f(glGetUniformLocation(textProg, "uColor"), 0.0f, 1.0f, 0.0f);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fontTex);

  glGenBuffers(1, &textVbo);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  {
    GLenum err = glGetError();
    if (err)
      fprintf(stderr, "GL error after blend enable: 0x%x\n", err);
  }

  glUseProgram(mainProg);
}

void drawText(float x, float y, const char *text, float scale, float *vertsOut,
              int *outLen) {
  int len = (int)strlen(text);
  if (len > 32)
    len = 32;
  *outLen = len;

  float charW = FONT_W * scale, charH = FONT_H * scale;
  for (int i = 0; i < len; i++) {
    int g;
    char c = text[i];
    if (c == ' ')
      g = 0;
    else if (c >= '0' && c <= '9')
      g = 1 + (c - '0');
    else if (c == ':')
      g = 11;
    else if (c == 'F')
      g = 12;
    else if (c == 'P')
      g = 13;
    else if (c == 'S')
      g = 14;
    else
      continue;

    float x0 = x + i * charW, y0 = y;
    float x1 = x0 + charW, y1 = y0 + charH;
    float tx0 = (float)g / NUM_GLYPH, tx1 = (float)(g + 1) / NUM_GLYPH;

    float *v = vertsOut + i * 24;
    v[0] = x0;
    v[1] = y1;
    v[2] = tx0;
    v[3] = 1.0f;
    v[4] = x1;
    v[5] = y1;
    v[6] = tx1;
    v[7] = 1.0f;
    v[8] = x0;
    v[9] = y0;
    v[10] = tx0;
    v[11] = 0.0f;
    v[12] = x1;
    v[13] = y1;
    v[14] = tx1;
    v[15] = 1.0f;
    v[16] = x0;
    v[17] = y0;
    v[18] = tx0;
    v[19] = 0.0f;
    v[20] = x1;
    v[21] = y0;
    v[22] = tx1;
    v[23] = 0.0f;
  }
}

void drawTextGl(const float *verts, int len, GLuint mainProg, int w, int h) {
  glUseProgram(textProg);
  glUniform2f(textUScreen, (float)w, (float)h);

  glBindBuffer(GL_ARRAY_BUFFER, textVbo);
  glBufferData(GL_ARRAY_BUFFER, len * 24 * sizeof(float), verts,
               GL_STREAM_DRAW);
  {
    GLenum err = glGetError();
    if (err)
      fprintf(stderr, "GL error after text buffer data: 0x%x\n", err);
  }
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (int)sizeof(float), 0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (int)sizeof(float),
                        (void *)(2LL * sizeof(float)));
  glEnableVertexAttribArray(1);
  {
    GLenum err = glGetError();
    if (err)
      fprintf(stderr, "GL error after text attrib setup: 0x%x\n", err);
  }

  glDrawArrays(GL_TRIANGLES, 0, 6 * len);
  {
    GLenum err = glGetError();
    if (err)
      fprintf(stderr, "GL error after text draw: 0x%x\n", err);
  }

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  glUseProgram(mainProg);
}
