#if !defined(__EMSCRIPTEN__)
#include "piece_library.hpp"
#include "../chess_assets.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "include/effects/SkImageFilters.h"
#include "include/core/SkPixmap.h"
#include <vector>

PieceLib::PieceLib() {
  for (int i = 0; i < (int)GameModel::ELEMS; i++)
    glow[i] = nullptr;
  for (int i = 0; i < (int)GameModel::ELEMS; i++) {
    const chess::Svg& sv = chess::pieces[i % (int)GameModel::ELEMS];
    SkMemoryStream stream(sv.data, sv.size, false);
    doms[i] = SkSVGDOM::MakeFromStream(stream);
    if (doms[i])
      doms[i]->setContainerSize(SkSize::Make(kBase, kBase));
  }
}

sk_sp<SkSVGDOM> PieceLib::dom(int idx) const {
  return (idx >= 0 && idx < (int)GameModel::ELEMS) ? doms[idx] : nullptr;
}

sk_sp<SkImage> PieceLib::glowAt(int idx) const {
  return (idx >= 0 && idx < (int)GameModel::ELEMS) ? glow[idx] : nullptr;
}

void PieceLib::buildGlow(int idx, const float* color) {
  if (idx < 0 || idx >= (int)GameModel::ELEMS || !doms[idx]) return;
  const int S = kGlowBase;
  SkImageInfo info = SkImageInfo::Make(S, S, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> pieceSurf = SkSurfaces::Raster(info);
  if (!pieceSurf) return;
  doms[idx]->setContainerSize(SkSize::Make(S, S));
  doms[idx]->render(pieceSurf->getCanvas());
  sk_sp<SkImage> piece = pieceSurf->makeImageSnapshot();
  SkPaint blur;
  blur.setImageFilter(SkImageFilters::Blur(kGlowSigma, kGlowSigma, SkTileMode::kClamp, nullptr));
  sk_sp<SkSurface> blurredSurf = SkSurfaces::Raster(info);
  blurredSurf->getCanvas()->clear(SkColors::kTransparent);
  blurredSurf->getCanvas()->drawImage(piece, 0.0f, 0.0f, SkSamplingOptions(), &blur);
  sk_sp<SkImage> blurred = blurredSurf->makeImageSnapshot();
  std::vector<uint8_t> buf((size_t)S * S * 4);
  std::vector<uint8_t> tmp((size_t)S * S * 4);
  std::vector<uint8_t> crispA((size_t)S * S);
  piece->readPixels(info, tmp.data(), info.minRowBytes(), 0, 0);
  for (size_t i = 0; i < (size_t)S * S; i++) crispA[i] = tmp[i * 4 + 3];
  blurred->readPixels(info, tmp.data(), info.minRowBytes(), 0, 0);
  const int er = (int)(color[0] * 255.f), eg = (int)(color[1] * 255.f), eb = (int)(color[2] * 255.f);
  for (size_t i = 0; i < (size_t)S * S; i++) {
    int a = (int)tmp[i * 4 + 3] - (int)crispA[i];
    if (a < 0) a = 0;
    buf[i * 4 + 0] = (uint8_t)((er * a) / 255);
    buf[i * 4 + 1] = (uint8_t)((eg * a) / 255);
    buf[i * 4 + 2] = (uint8_t)((eb * a) / 255);
    buf[i * 4 + 3] = (uint8_t)a;
  }
  glow[idx] = [&]() -> sk_sp<SkImage> {
    sk_sp<SkSurface> gp = SkSurfaces::Raster(info);
    if (!gp) return nullptr;
    gp->writePixels(SkPixmap(info, buf.data(), info.minRowBytes()), 0, 0);
    return gp->makeImageSnapshot();
  }();
}
#endif
