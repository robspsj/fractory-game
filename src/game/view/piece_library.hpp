#pragma once
#if !defined(__EMSCRIPTEN__)
#include <array>
#include "include/core/SkImage.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "../model/game_model.hpp"

struct PieceLib {
  static constexpr float kBase = 72.0f;
  static constexpr int   kGlowBase = 128;
  static constexpr float kGlowSigma = 5.5f;
  std::array<sk_sp<SkSVGDOM>, GameModel::ELEMS> doms;
  std::array<sk_sp<SkImage>, GameModel::ELEMS> glow;

  PieceLib();
  sk_sp<SkSVGDOM> dom(int idx) const;
  sk_sp<SkImage> glowAt(int idx) const;
  void buildGlow(int idx, const float* color);
};
#endif
