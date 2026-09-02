#pragma once
#if !defined(__EMSCRIPTEN__)
#include <array>
#include <string>
#include "modules/svg/include/SkSVGDOM.h"
#include "types.hpp"

std::string resolveAsset(const char *path);

struct UiLib {
  static constexpr int kFamilies = 7;
  std::array<sk_sp<SkSVGDOM>, kFamilies> bg;
  std::array<sk_sp<SkSVGDOM>, kFamilies> cell;
  UiLib();
  static sk_sp<SkSVGDOM> load(const char *path);
};

void renderTile(class SkCanvas *canvas, const sk_sp<SkSVGDOM> &dom, struct Rect r);
#endif
