#if !defined(__EMSCRIPTEN__)
#include "ui_library.hpp"
#include <cstdio>
#include <vector>
#include <filesystem>
#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"

std::string resolveAsset(const char *path) {
  namespace fs = std::filesystem;
  fs::path rel(path);
  std::error_code ec;
  if (fs::exists(rel, ec)) return rel.string();
  fs::path dir = fs::current_path(ec);
  while (true) {
    fs::path cand = dir / rel;
    if (fs::exists(cand, ec)) return cand.string();
    if (!dir.has_parent_path()) break;
    dir = dir.parent_path();
  }
  return std::string(path);
}

UiLib::UiLib() {
  for (int i = 0; i < kFamilies; i++) {
    char p[64];
    snprintf(p, sizeof(p), "assets/ui/bg_%d.svg", i);
    bg[i] = load(p);
    snprintf(p, sizeof(p), "assets/ui/cell_%d.svg", i);
    cell[i] = load(p);
  }
}

sk_sp<SkSVGDOM> UiLib::load(const char *path) {
  FILE *f = fopen(resolveAsset(path).c_str(), "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) { fclose(f); return nullptr; }
  std::vector<char> buf((size_t)n);
  bool ok = fread(buf.data(), 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  if (!ok) return nullptr;
  SkMemoryStream stream(buf.data(), (size_t)n, false);
  return SkSVGDOM::MakeFromStream(stream);
}

void renderTile(SkCanvas *canvas, const sk_sp<SkSVGDOM> &dom, Rect r) {
  if (!dom || !canvas) return;
  if (r.outsideClip() || r.w < 0.002f || r.h < 0.002f) return;
  dom->setContainerSize(SkSize::Make(r.w, r.h));
  canvas->save();
  canvas->translate((SkScalar)r.ox, (SkScalar)r.oy);
  dom->render(canvas);
  canvas->restore();
}
#endif
