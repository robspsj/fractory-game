// Embedded SVG icon data for the WASM build. See tools/gen_wasm_svg_assets.py
// for how generated/wasm_svg_assets.h is produced (compact pre-normalized
// path data — SkSVGDOM/expat and the filesystem are both unavailable on web).
#if defined(__EMSCRIPTEN__)
#include "asset_manager.hpp"
#include "game_view.hpp"
#include "per_path_svg.hpp"
#include <cstdio>
#include "generated/wasm_svg_assets.h"
#include "include/core/SkColor.h"
#include "include/core/SkPath.h"
#include "include/utils/SkParsePath.h"

std::unique_ptr<PerPathSvg> AssetManager::loadEmbeddedSvg(const std::string &path) {
  const wasm_svg::Icon *icon = nullptr;
  for (size_t i = 0; i < wasm_svg::kIconCount; i++) {
    if (path == wasm_svg::kIcons[i].path) { icon = &wasm_svg::kIcons[i]; break; }
  }
  if (!icon || icon->count == 0) return nullptr;
  auto result = std::make_unique<PerPathSvg>();
  result->viewBoxW = icon->vbW;
  result->viewBoxH = icon->vbH;
  for (size_t i = 0; i < icon->count; i++) {
    const wasm_svg::Shape &sh = icon->shapes[i];
    auto opt = SkParsePath::FromSVGString(sh.d);
    if (!opt) continue;
    PerPathSvg::PathInfo pi;
    pi.path = *opt;
    pi.paint.setAntiAlias(true);
    pi.paint.setColor(SkColorSetRGB(sh.r, sh.g, sh.b));
    if (sh.style == wasm_svg::kStroke) {
      pi.paint.setStyle(SkPaint::kStroke_Style);
      pi.paint.setStrokeWidth(sh.strokeW > 0 ? sh.strokeW : 1.0f);
    } else {
      pi.paint.setStyle(SkPaint::kFill_Style);
    }
    pi.bounds = pi.path.getBounds();
    result->paths.push_back(std::move(pi));
  }
  if (result->paths.empty()) return nullptr;
  return result;
}

void AssetManager::loadEmbeddedItemConfig(GameView &view) {
  for (size_t i = 0; i < wasm_svg::kItemCount; i++) {
    const wasm_svg::Item &it = wasm_svg::kItems[i];
    GameView::ItemRenderInfo info;
    info.color[0] = it.r; info.color[1] = it.g; info.color[2] = it.b;
    info.icon = it.icon;
    if (it.svgIdx >= 0 && (size_t)it.svgIdx < wasm_svg::kIconCount)
      info.svgPath = wasm_svg::kIcons[it.svgIdx].path;
    info.sway = (it.sway == wasm_svg::kPerPath) ? GameView::SwayMode::PerPath : GameView::SwayMode::Static;
    view._itemRenderInfo.emplace(it.id, std::move(info));
  }
  fprintf(stderr, "[GameView] Loaded %zu item configs from embedded data\n", view._itemRenderInfo.size());
}
#endif
