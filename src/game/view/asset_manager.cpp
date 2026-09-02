#include "asset_manager.hpp"
#include "game_view.hpp"
#include "piece_library.hpp"
#include "ui_library.hpp"
#include <cctype>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "include/core/SkData.h"
#include "include/core/SkStream.h"

void AssetManager::loadItemConfig(GameView &view, const std::string &path) {
  view._itemRenderInfo.clear();
  std::string tryPath = path;
  if (!std::filesystem::exists(tryPath)) tryPath = "../" + path;
  std::ifstream in(tryPath);
  if (!in) {
    std::cout << "[GameView] WARNING: could not open item config \"" << tryPath << "\"" << std::endl;
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) continue;
    line = line.substr(start);
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> f;
    size_t pos = 0;
    while (pos <= line.size()) {
      size_t comma = line.find(',', pos);
      std::string tok = (comma == std::string::npos) ? line.substr(pos) : line.substr(pos, comma - pos);
      size_t a = tok.find_first_not_of(" \t");
      size_t b = tok.find_last_not_of(" \t");
      tok = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
      f.push_back(tok);
      if (comma == std::string::npos) break;
      pos = comma + 1;
    }
    if (f.size() < 6) continue;
    try {
      int id = std::stoi(f[0]);
      float r = std::stof(f[2]), g = std::stof(f[3]), b = std::stof(f[4]);
      int icon = std::stoi(f[5]);
      GameView::ItemRenderInfo info;
      info.color[0] = r; info.color[1] = g; info.color[2] = b;
      info.icon = icon;
      info.svgPath = (f.size() > 6) ? f[6] : "";
      // Optional 8th column: sway mode — "static" or "perPath" (also 0/1, false/true)
      if (f.size() > 7 && !f[7].empty()) {
        std::string s = f[7];
        for (auto &c : s) c = std::tolower(c);
        if (s == "perpath" || s == "per_path" || s == "per-path" || s == "1" || s == "true" || s == "yes" || s == "sway")
          info.sway = GameView::SwayMode::PerPath;
        else
          info.sway = GameView::SwayMode::Static;
      } else {
        info.sway = GameView::SwayMode::Static; // backward compat: no column → static
      }
      view._itemRenderInfo.emplace(id, std::move(info));
    } catch (...) { continue; }
  }
  std::cout << "[GameView] Loaded " << view._itemRenderInfo.size() << " item configs from \"" << tryPath << "\"" << std::endl;
}

SkSVGDOM *AssetManager::getSvgDom(GameView &view, const std::string &path) {
  if (path.empty()) return nullptr;
  auto it = view._svgCache.find(path);
  if (it != view._svgCache.end()) return it->second.get();
  sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
  if (!data) {
    std::string alt = "../" + path;
    if (std::filesystem::exists(alt)) data = SkData::MakeFromFileName(alt.c_str());
  }
  if (!data) return nullptr;
  SkMemoryStream stream(data);
  sk_sp<SkSVGDOM> dom = SkSVGDOM::MakeFromStream(stream);
  if (!dom) return nullptr;
  SkSVGDOM* raw = dom.get();
  view._svgCache[path] = std::unique_ptr<SkSVGDOM>(dom.release());
  return raw;
}

#if !defined(__EMSCRIPTEN__)
void AssetManager::loadChessAssets(GameView &view) {
  delete view._pieces;
  view._pieces = new PieceLib();
  int ok = 0;
  for (int i = 0; i < (int)GameModel::ELEMS; i++) {
    if (view._pieces->doms[i]) { ok++; view._pieces->buildGlow(i, view._elemColors[i]); }
  }
  fprintf(stderr, "loadChessAssets: %d/%d piece SVGs parsed (glow baked)\n", ok, (int)GameModel::ELEMS);
}

void AssetManager::loadUiAssets(GameView &view) {
  delete view._ui;
  view._ui = new UiLib();
  int ok = 0;
  for (int i = 0; i < UiLib::kFamilies; i++) if (view._ui->bg[i] && view._ui->cell[i]) ok++;
  fprintf(stderr, "loadUiAssets: %d/%d tile SVGs parsed\n", ok, (int)UiLib::kFamilies);
}
#endif
