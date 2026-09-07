#pragma once
#include <string>
#include <memory>
class GameView;
class SkSVGDOM;
struct PerPathSvg;

class AssetManager {
 public:
  static void loadItemConfig(GameView &view, const std::string &path = "assets/items.csv");
  static SkSVGDOM *getSvgDom(GameView &view, const std::string &path);
  static void loadChessAssets(GameView &view);
  static void loadUiAssets(GameView &view);
#if defined(__EMSCRIPTEN__)
  // WASM has no SkSVGDOM (Skia built without expat) and no filesystem, so
  // icons come from the compact blob generated at build time by
  // tools/gen_wasm_svg_assets.py into generated/wasm_svg_assets.h.
  static std::unique_ptr<PerPathSvg> loadEmbeddedSvg(const std::string &path);
  static void loadEmbeddedItemConfig(GameView &view);
#endif
};
