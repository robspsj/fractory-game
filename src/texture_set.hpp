#pragma once
#include "gl.hpp"

// Texture atlas tile indices
enum TileIndex : int {
  TILE_EMPTY = 0,
  TILE_ELEM_0 = 1,  // blue
  TILE_ELEM_1 = 2,  // red
  TILE_ELEM_2 = 3,  // green
  TILE_ELEM_3 = 4,  // yellow
  TILE_ELEM_4 = 5,  // purple
  TILE_STATION = 6,
  TILE_RSV_INPUT = 7,
  TILE_RSV_OUTPUT = 8,
  TILE_RSV_BUFFER = 9,
  TILE_WHITE = 10,    // solid white — use for non-textured quads (color-only)
  TILE_COUNT = 11
};

class TextureSet {
public:
  TextureSet() = default;
  ~TextureSet();

  // Load atlas from PNG. tileW/tileH = size of each tile in pixels.
  bool loadAtlas(const char *path, int tileW, int tileH);

  // Get UV rect for a tile index: {u0, v0, u1, v1}
  void getUV(int tileIndex, float &u0, float &v0, float &u1, float &v1) const;

  GLuint texture() const { return _tex; }
  int atlasWidth() const { return _atlasW; }
  int atlasHeight() const { return _atlasH; }
  int tileWidth() const { return _tileW; }
  int tileHeight() const { return _tileH; }
  int tileCount() const { return _tileCount; }
  bool loaded() const { return _tex != 0; }

private:
  GLuint _tex = 0;
  int _atlasW = 0, _atlasH = 0;
  int _tileW = 0, _tileH = 0;
  int _tileCount = 0;
};
