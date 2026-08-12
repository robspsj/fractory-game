#include "texture_set.hpp"
#include "vendor/stb_image.h"
#include <cstdio>

TextureSet::~TextureSet() {
  if (_tex)
    glDeleteTextures(1, &_tex);
}

bool TextureSet::loadAtlas(const char *path, int tileW, int tileH) {
  int channels;
  stbi_set_flip_vertically_on_load(true);
  // Force RGBA loading
  unsigned char *data = stbi_load(path, &_atlasW, &_atlasH, &channels, 4);
  if (!data) {
    fprintf(stderr, "TextureSet: failed to load '%s': %s\n", path, stbi_failure_reason());
    return false;
  }

  _tileW = tileW;
  _tileH = tileH;
  _tileCount = _atlasW / tileW;

  glGenTextures(1, &_tex);
  glBindTexture(GL_TEXTURE_2D, _tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _atlasW, _atlasH, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  GLenum err = glGetError();
  if (err)
    fprintf(stderr, "TextureSet: GL error after upload: 0x%x\n", err);

  stbi_image_free(data);
  fprintf(stderr, "TextureSet: loaded '%s' (%dx%d, %d tiles of %dx%d)\n",
          path, _atlasW, _atlasH, _tileCount, tileW, tileH);
  return true;
}

void TextureSet::getUV(int tileIndex, float &u0, float &v0, float &u1, float &v1) const {
  if (_atlasW <= 0 || tileIndex < 0 || tileIndex >= _tileCount) {
    u0 = v0 = 0.0f;
    u1 = v1 = 1.0f;
    return;
  }
  float tileUVW = (float)_tileW / (float)_atlasW;
  u0 = tileIndex * tileUVW;
  u1 = u0 + tileUVW;
  v0 = 0.0f;
  v1 = 1.0f;
}
