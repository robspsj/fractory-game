#include "particle.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPoint.h"
#include "include/core/SkSpan.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// Config: the extension point. Uniform default for every kind / item id for now,
// but structured so per-situation / per-item overrides plug in here later.
PuffConfig PuffConfig::cfgFor(EffectKind, int) {
  PuffConfig c;
  (void)c;
  return c;
}

// ---------------------------------------------------------------------------
// Deterministic irregular-blob radius (unit-space silhouettes for the smoke).
static float blobRadius(float th, float seed) {
  float r = 1.0f;
  r += 0.14f * std::sin(th * 2.0f + seed);
  r += 0.09f * std::sin(th * 3.0f + seed * 1.7f);
  r += 0.06f * std::cos(th * 5.0f + seed * 2.3f);
  r += 0.05f * std::sin(th * 7.0f + seed * 0.7f);
  r += 0.04f * std::sin(th * 11.0f + seed * 1.3f);
  return r; // ~0.62..1.38 about a centre of 1.0
}

ParticleSystem::ParticleSystem() {
  buildBlobs();
  _sources.reserve(512);
}

ParticleSystem::~ParticleSystem() = default; // SkShader is complete here

void ParticleSystem::buildBlobs() {
  constexpr int N = 42; // perimeter samples
  for (int k = 0; k < kBlobs; k++) {
    const float seed = (float)k * 1.618f + 2.71f;
    SkPathBuilder b;
    for (int i = 0; i < N; i++) {
      float th = (float)(2.0 * 3.141592653589793 * i) / (float)N;
      float rad = blobRadius(th, seed);
      float x = std::cos(th) * rad;
      float y = std::sin(th) * rad;
      if (i == 0) b.moveTo(x, y);
      else b.lineTo(x, y);
    }
    b.close();
    _blobs[k] = b.detach();
  }
}

sk_sp<SkShader> ParticleSystem::gradientFor(int elem, const float color[3]) {
  auto it = _grads.find(elem);
  if (it != _grads.end()) return it->second;
  SkColor4f cols[3] = {
      {color[0], color[1], color[2], 1.00f},
      {color[0], color[1], color[2], 0.52f},
      {color[0], color[1], color[2], 0.00f},
  };
  float pos[3] = {0.0f, 0.72f, 1.0f};
  SkGradient::Colors gc = SkGradient::Colors(
      SkSpan<const SkColor4f>(cols, 3), SkSpan<const float>(pos, 3),
      SkTileMode::kClamp);
  SkGradient grad(gc, SkGradient::Interpolation{});
  sk_sp<SkShader> shader =
      SkShaders::RadialGradient(SkPoint::Make(0.0f, 0.0f), 1.0f, grad, nullptr);
  _grads.emplace(elem, shader);
  return shader;
}

// ---------------------------------------------------------------------------
// Per-puff derivation (stateless). Every property of puff `i` in a burst is a
// pure function of (seed, i, age). No per-puff memory is stored anywhere.
static inline double puffAngle(int seed, int i) {
  return (double)(((uint32_t)(seed + i) * 1271u + 32749u) % 6283) / 1000.0;
}
static inline double puffRand(int seed, int i) {
  return (double)(((uint32_t)(seed + i) * 311u + 53u) % 100) / 100.0;
}
static inline float puffVyFactor(int seed, int i) {
  return 0.6f + (float)(((uint32_t)(seed + i) * 17u) % 100) / 100.0f;
}
static inline float puffRot0(int seed, int i) {
  return (float)(((uint32_t)(seed + i) * 113u) % 3600) / 10.0f;
}
static inline float puffRotSpeed(int seed, int i, float swirl) {
  return swirl * ((float)(((uint32_t)(seed + i) * 37u) % 40) / 20.0f - 1.0f);
}

void ParticleSystem::spawn(float wx, float wy, float cellHalfWorld,
                           const float color[3], EffectKind kind, int itemId) {
  if (!color) return;
  const PuffConfig cfg = PuffConfig::cfgFor(kind, itemId);
  if (_sources.size() >= (size_t)kMaxSources) return;      // pool full
  gradientFor(itemId, color);                              // warm tint cache
  Source s;
  s.x = wx;
  s.y = wy;
  s.half = cellHalfWorld;
  s.elem = itemId;
  s.seed = ++_seedCounter;
  s.itemId = itemId;
  s.kind = kind;
  s.startTime = _now;
  s.cfg = cfg;
  _sources.push_back(s);
}

void ParticleSystem::update(double dt) {
  _now += dt;
  _sources.erase(std::remove_if(_sources.begin(), _sources.end(),
                                [&](const Source &s) {
                                  return (_now - s.startTime) >= s.cfg.life;
                                }),
                 _sources.end());
}

void ParticleSystem::draw(SkCanvas *canvas, float zoom, float panX, float panY,
                          float aspect, int winW, int winH) const {
  _lastVisible = 0;
  if (_sources.empty() || !canvas) return;
  const float W = (float)winW, H = (float)winH;
  const float scalePx = 0.5f * std::min(W, H);

  for (const Source &s : _sources) {
    const double age = std::max(0.0, _now - s.startTime);
    const float t = (float)(age / s.cfg.life);     // 0..1 progress
    if (t >= 1.0f) continue;
    const float fade = s.cfg.alpha0 * (1.0f - t);
    if (fade <= 0.0f) continue;
    const float sizeNow = s.half * (s.cfg.size0 + (s.cfg.size1 - s.cfg.size0) * t);

    // Derive and draw this burst's `count` puffs from (seed, i, age).
    for (int i = 0; i < s.cfg.count; i++) {
      const double ang = puffAngle(s.seed, i);
      const float scat = s.cfg.spread * s.half * (float)puffRand(s.seed, i);
      const float wx = s.x + (float)std::cos(ang) * scat;            // lateral scatter
      const float wy = s.y + (float)std::sin(ang) * scat
                       + s.cfg.driftUp * s.half * 2.0f * puffVyFactor(s.seed, i) * (float)age; // rise

      // world -> NDC through the CURRENT camera (tracks pan/zoom).
      const float nx = wx * zoom + panX;
      const float ny = wy * aspect * zoom + panY;
      const float px = (nx + 1.0f) * 0.5f * W;
      const float py = (1.0f - ny) * 0.5f * H;
      const float rpix = sizeNow * zoom * scalePx;
      if (rpix <= 0.02f) continue;

      SkPaint paint;
      paint.setAntiAlias(true);
      auto it = _grads.find(s.elem);
      if (it != _grads.end()) paint.setShader(it->second);
      paint.setAlpha((uint8_t)std::lround(fade * 255.0f));

      _lastVisible++;
      canvas->save();
      canvas->resetMatrix();
      canvas->translate(px, py);
      canvas->scale(rpix, rpix);
      canvas->rotate(puffRot0(s.seed, i) + puffRotSpeed(s.seed, i, s.cfg.swirl) * (float)age);
      canvas->drawPath(_blobs[i % kBlobs], paint);
      canvas->restore();
    }
  }
}

void ParticleSystem::clear() {
  _sources.clear();
  _lastVisible = 0;
}

std::size_t ParticleSystem::allocatedBytes() const {
  std::size_t bytes = _sources.capacity() * sizeof(Source);
  bytes += _grads.size() * 64;
  for (const SkPath &p : _blobs) bytes += p.countPoints() * sizeof(SkPoint) * 2;
  return bytes;
}