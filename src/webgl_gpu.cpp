// WASM GPU present path: Ganesh + WebGL2 (Emscripten WebIDL). Isolated from
// main.cpp the same way metal_gpu.cpp is, so Skia's GPU headers (and their
// GL_*/Rect clash) stay out of the game code.
#if defined(__EMSCRIPTEN__)
#include <emscripten/html5_webgl.h>
#include <cstdint>
#include <webgl/webgl1.h>
#include <webgl/webgl1_ext.h>
#include <webgl/webgl2.h>
#include <webgl/webgl2_ext.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

// GL constants (numeric: avoids header enum differences between GLES/WebGL).
#define FBO_FRAMEBUFFER      0x8D40  // GL_FRAMEBUFFER
#define FBO_SAMPLES          0x8AF5  // GL_SAMPLES
#define FBO_STENCIL_BITS     0x0D00  // GL_STENCIL_BITS
#define FBO_CLEAR_COLOR_BIT  0x00004000
#define FBO_CLEAR_STENCIL_BIT 0x00000100
#define FBO_RGBA8            0x8058  // GL_RGBA8 internal format

typedef void (*FrctRenderFn)(void*, SkCanvas*, int, int);

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE s_glCtx = 0;
static sk_sp<GrDirectContext> s_grCtx;
static sk_sp<SkSurface> s_surface;
static int s_surfW = 0, s_surfH = 0;

extern "C" bool frct_webgl_init(void) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  attrs.minorVersion = 0;
  attrs.alpha = 0;
  attrs.depth = 0;
  attrs.stencil = 1;
  attrs.antialias = 0;              // Skia does its own AA
  attrs.premultipliedAlpha = 1;
  attrs.preserveDrawingBuffer = 0;  // the compositor reclaims it each frame
  attrs.failIfMajorPerformanceCaveat = 0;
  attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
  s_glCtx = emscripten_webgl_create_context("#canvas", &attrs);
  if ((intptr_t)s_glCtx <= 0) return false;
  if (emscripten_webgl_make_context_current(s_glCtx) != EMSCRIPTEN_RESULT_SUCCESS) return false;
  auto iface = GrGLInterfaces::MakeWebGL();
  if (!iface) return false;
  s_grCtx = GrDirectContexts::MakeGL(iface);
  return s_grCtx != nullptr;
}

extern "C" bool frct_webgl_frame(FrctRenderFn render, void* ud, int w, int h) {
  if (!s_grCtx) return false;
  if (!s_surface || s_surfW != w || s_surfH != h ||
      emscripten_webgl_get_current_context() != s_glCtx) {
    // Recreate the surface (and rebind after any context juggling).
    emscripten_webgl_make_context_current(s_glCtx);
    emscripten_glBindFramebuffer(FBO_FRAMEBUFFER, 0);
    emscripten_glClearColor(0, 0, 0, 0);
    emscripten_glClearStencil(0);
    emscripten_glClear(FBO_CLEAR_COLOR_BIT | FBO_CLEAR_STENCIL_BIT);
    s_grCtx->resetContext(kRenderTarget_GrGLBackendState | kMisc_GrGLBackendState);
    int samples = 0, stencil = 0;
    emscripten_glGetIntegerv(FBO_SAMPLES, &samples);
    emscripten_glGetIntegerv(FBO_STENCIL_BITS, &stencil);
    GrGLFramebufferInfo info;
    info.fFBOID = 0;  // the on-screen canvas default framebuffer
    info.fFormat = FBO_RGBA8;
    auto rt = GrBackendRenderTargets::MakeGL(w, h, samples, stencil, info);
    s_surface = SkSurfaces::WrapBackendRenderTarget(s_grCtx.get(), rt,
                                                    kBottomLeft_GrSurfaceOrigin,
                                                    kRGBA_8888_SkColorType,
                                                    SkColorSpace::MakeSRGB(), nullptr);
    if (!s_surface) return false;
    s_surfW = w;
    s_surfH = h;
  }
  render(ud, s_surface->getCanvas(), w, h);
  s_grCtx->flushAndSubmit();
  return true;
}

extern "C" void frct_webgl_shutdown(void) {
  s_surface.reset();
  s_grCtx.reset();
  if (s_glCtx) emscripten_webgl_destroy_context(s_glCtx);
  s_glCtx = 0;
}
#endif
