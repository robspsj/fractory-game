#pragma once
#include <string>
class GameView;
class SkSVGDOM;

class AssetManager {
 public:
  static void loadItemConfig(GameView &view, const std::string &path = "assets/items.csv");
  static SkSVGDOM *getSvgDom(GameView &view, const std::string &path);
  static void loadChessAssets(GameView &view);
  static void loadUiAssets(GameView &view);
};
