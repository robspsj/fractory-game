#include "svg_assets.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

// svg2b2d's public entry points (linked from third_party/svg2b2d/svg.cpp).
extern "C" bool parseSVG(const void* bytes, size_t sz, BLImage& outImage);
extern "C" bool parseSVGscaled(const void* bytes, size_t sz, int targetW, int targetH, BLImage& outImage);

// Same theoretical reason (see svgFillRadialGradient below): classic Blend2D
// v0.0.18's non-JIT reference rasterizer cannot rasterize radial gradients
// (only solid + linear), so svg2b2d's gradient fills come out flat/broken. We
// rewrite `fill="url(#id)"` to a solid sentinel (`#FF00FE`, magenta) before
// parsing, then software-fill those pixels with the true radial gradient.

// Simple number parse (no acknowledgment of locales). Skips whitespace and
// quote chars so we parse `"40%"`, `"0%"` etc. directly.
static const char* svgSkipNum(const char* s, double* out) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' || *s == '"' || *s == '\'') s++;
  char* end = nullptr;
  double v = strtod(s, &end);
  if (end == s) { *out = std::nan(""); return nullptr; }
  if (out) *out = v;
  return end;
}

// Extract radial-gradient parameters + stop colors from elementN.svg.
// Returns false if no radial gradient found (caller falls back to intrinsic parse).
struct SvgRadialGrad {
  bool present = false;
  double cx = 0.5, cy = 0.5, r = 1.0;         // normalized [0..1] within icon box
  uint32_t stop0 = 0xFFFFFFFF, stop1 = 0xFF000000;  // ARGB at offset 0 and 1
};
static SvgRadialGrad svgParseRadialGradient(const uint8_t* data, size_t size) {
  SvgRadialGrad g;
  std::string s((const char*)data, size);
  auto rgStart = s.find("<radialGradient");
  if (rgStart == std::string::npos) return g;
  auto rgEnd = s.find("</radialGradient>", rgStart);
  if (rgEnd == std::string::npos)
    rgEnd = s.size();
  std::string body = s.substr(rgStart, rgEnd - rgStart);

  // cx, cy, r attributes (may be '%' or user units).
  auto grab = [&](const char* attr, double def) {
    std::string key(attr);
    key += "=";
    auto p = body.find(key);
    if (p == std::string::npos) return def;
    p += key.size();
    while (p < body.size() && (body[p] == '"' || body[p] == '\'')) p++;
    double v = def;
    const char* q = svgSkipNum(body.c_str() + p, &v);
    if (!q) return def;
    // If it's a percentage, divide by 100 (normalized). If plain units, keep as-is.
    const char* endp = q;
    while (*endp == ' ' || *endp == '\t') endp++;
    if (*endp == '%') return v / 100.0;
    return v;
  };
  g.cx = grab("cx", 0.5);
  g.cy = grab("cy", 0.5);
  g.r = grab("r", 1.0);

  // stop-color values: scan all <stop ...> in body for offset 0 and 1.
  size_t pos = 0;
  while ((pos = body.find("<stop", pos)) != std::string::npos) {
    auto stopEnd = body.find('>', pos);
    std::string st = body.substr(pos, stopEnd - pos);
    // offset
    double off = 0.0;
    auto offp = st.find("offset");
    if (offp != std::string::npos) {
      auto valp = st.find('=', offp);
      const char* q = svgSkipNum(st.c_str() + valp + 1, &off);
      if (q) { const char* e = q; while (*e==' '||*e=='\t') e++; if (*e=='%') off/=100.0; }
    }
    // stop-color
    auto sp = st.find("stop-color");
    if (sp != std::string::npos) {
      auto vp = st.find('=', sp);
      std::string hex = st.substr(vp + 1);
      while (!hex.empty() && (hex[0]==' '||hex[0]=='"'||hex[0]=='\'')) hex.erase(0,1);
      while (!hex.empty() && (hex.back()=='"'||hex.back()=='\''||hex.back()=='\r')) hex.pop_back();
      unsigned rr=0, gg=0, bb=0;
      if (hex.size() >= 7 && hex[0]=='#' && sscanf(hex.c_str(), "#%2x%2x%2x", &rr,&gg,&bb)==3) {
        uint32_t c = (0xFFu<<24)|(rr<<16)|(gg<<8)|bb;
        if (off <= 0.5) g.stop0 = c; else g.stop1 = c;
      }
    }
    pos = stopEnd + 1;
  }
  g.present = true;
  return g;
}

// Software radial-gradient fill over all sentinel-colored opaque pixels.
// target: PRGB32 image (premultiplied). Grad geometry is normalized within the
// image (cx,cy = center, r = radius as a fraction of min dimension).
static void blendGradPixel(uint32_t &dst, const float c0[3], const float c1[3], double t, uint32_t alpha) {
  double tf = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
  double r  = c0[0] + (c1[0]-c0[0])*tf;
  double g  = c0[1] + (c1[1]-c0[1])*tf;
  double b  = c0[2] + (c1[2]-c0[2])*tf;
  // Premultiply by alpha (0..255) to match PRGB32.
  double a = alpha / 255.0;
  int rr = (int)(r * a * 255.0 + 0.5), gg = (int)(g * a * 255.0 + 0.5), bb = (int)(b * a * 255.0 + 0.5);
  if (rr>255)rr=255; if(gg>255)gg=255; if(bb>255)bb=255;
  dst = ((uint32_t)alpha<<24)|((uint32_t)rr<<16)|((uint32_t)gg<<8)|(uint32_t)bb;
}

static void svgFillRadialGradient(BLImage& img, const SvgRadialGrad& g) {
  BLImageData d;
  if (img.getData(&d) != BL_SUCCESS || !d.pixelData) return;
  const int W = d.size.w, H = d.size.h;
  if (W <= 0 || H <= 0) return;
  uint32_t* px = (uint32_t*)d.pixelData;
  const size_t stride = (size_t)d.stride;

  double c0r = ((g.stop0>>16)&0xFF)/255.0, c0g = ((g.stop0>>8)&0xFF)/255.0, c0b = (g.stop0&0xFF)/255.0;
  double c1r = ((g.stop1>>16)&0xFF)/255.0, c1g = ((g.stop1>>8)&0xFF)/255.0, c1b = (g.stop1&0xFF)/255.0;
  const float C0[3] = {(float)c0r,(float)c0g,(float)c0b}, C1[3] = {(float)c1r,(float)c1g,(float)c1b};

  // Center in pixels, radius in pixels (ratio to smaller of W/H so circular).
  const double cxPx = g.cx * W;
  const double cyPx = g.cy * H;
  const double minDim = std::min(W, H);
  const double rPx = g.r * minDim;
  if (rPx <= 0.0001) return;

  // Sentinel fill is #FF00FE (R=FF,G=00,B=FE, magenta). Match by hue (R & B high,
  // G low) so premultiplied/blended edge pixels are still caught regardless of
  // brightness and regardless of small R-vs-B order differences.
  for (int y = 0; y < H; y++) {
    uint32_t* row = (uint32_t*)((uint8_t*)px + y*stride);
    for (int x = 0; x < W; x++) {
      uint32_t a = row[x] >> 24;
      if (a == 0) continue;
      uint8_t R = (uint8_t)(row[x]>>16), G = (uint8_t)(row[x]>>8), B = (uint8_t)row[x];
      // magenta: R and B substantially above G.
      if (R >= G + 120 && B >= G + 120 && (uint32_t)(R + B) >= 192) {
        double dx = x - cxPx + 0.5, dy = y - cyPx + 0.5;
        double dist = std::sqrt(dx*dx + dy*dy);
        blendGradPixel(row[x], C0, C1, dist / rPx, a);
      }
    }
  }
}

bool svgLoadFromBytes(const void* data, size_t size, BLImage& out) {
  if (!data || size == 0)
    return false;
  return parseSVG(data, size, out);
}

bool svgLoadFromBytesAtSize(const void* data, size_t size, int targetW, int targetH, BLImage& out) {
  if (!data || size == 0)
    return false;
  // The NO_JIT Blend2D cannot rasterize radial gradients, so we mark the
  // gradient-filled regions with a solid sentinel color and rasterize the
  // gradient in software afterwards. Rewrite fill="url(#X)" -> sentinel.
  std::string svg((const char*)data, size);
  // Replace `fill="url(#id)"` (and only that) with the sentinel. The closing
  // quote is the SECOND `"` after the `fill="` opener.
  size_t p = 0;
  while ((p = svg.find("fill=\"url(#", p)) != std::string::npos) {
    size_t firstQuote = p + 5;  // the `"` right after `fill=`
    size_t closeQuote = svg.find('"', firstQuote + 1);
    if (closeQuote != std::string::npos) {
      // Replace `fill=  "url(#... )  "  ` -> `fill="  #FF00FE  "`
      svg.replace(p, closeQuote - p + 1, "fill=\"#FF00FE\"");
      p = closeQuote + 1;
    } else {
      p = svg.size();
    }
  }
  if (!parseSVGscaled(svg.data(), svg.size(), targetW, targetH, out))
    return false;
  // Render the radial gradient fill in software (NO_JIT Blend2D can't do radial).
  SvgRadialGrad g = svgParseRadialGradient((const uint8_t*)data, size);
  if (g.present)
    svgFillRadialGradient(out, g);
  return true;
}