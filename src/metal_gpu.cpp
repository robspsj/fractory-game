// Native GPU present path (Skia Ganesh. Metal). Compiled C++ ONLY — it must NOT
// include any game headers here, because the Skia GPU/Metal headers pull
// MacTypes' global `struct Rect`, which collides with the game's own `Rect`.
// main.cpp hands rendering back to this TU via a callback (void*, SkCanvas*, w, h).
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlTypes.h"
#include <cstdio>

// Metal window glue (metal_glue.mm): retained CFTypeRef handles.
extern "C" {
void* frct_metal_create(void*, void**, void**, int);
void* frct_metal_begin_frame(void*, void**, int*, int*, unsigned*);
void  frct_metal_present(void*, void*);
void  frct_metal_release(void*);
void  frct_metal_destroy(void*);
}

static void*                  s_metalCtx = nullptr;
static sk_sp<GrDirectContext> s_dctx;

extern "C" {

// Create the MTLDevice/queue + CAMetalLayer view + Ganesh Metal context. `vsync`
// toggles the layer's displaySyncEnabled (on = throttled to refresh, off = unlocked).
bool frct_gpu_init(void* win, bool vsync) {
  void* dev = nullptr, *que = nullptr;
  s_metalCtx = frct_metal_create(win, &dev, &que, vsync ? 1 : 0);
  if (!s_metalCtx) {
    fprintf(stderr, "GPU: Metal init failed; using CPU present\n");
    return false;
  }
  GrMtlBackendContext bc;
  bc.fDevice = sk_cfp<GrMTLHandle>((GrMTLHandle)dev);   // adopt +1
  bc.fQueue  = sk_cfp<GrMTLHandle>((GrMTLHandle)que);
  s_dctx = GrDirectContexts::MakeMetal(bc);
  if (!s_dctx) {
    fprintf(stderr, "GPU: GrDirectContext failed; using CPU present\n");
    frct_metal_destroy(s_metalCtx);
    s_metalCtx = nullptr;
    return false;
  }
  return true;
}

// Render the current Metal drawable: caller fills it via `render(ctx, canvas, w, h)`,
// then the frame is flushed and presented (vsync via the drawable).
bool frct_gpu_frame(void (*render)(void*, SkCanvas*, int, int), void* ctx) {
  static bool s_warned;   // one-time diagnostics
  void* tex = nullptr;
  int w = 0, h = 0;
  unsigned fmt = 0;
  void* dr = frct_metal_begin_frame(s_metalCtx, &tex, &w, &h, &fmt);
  if (!dr || !tex || w <= 0 || h <= 0) {
    if (!s_warned) { fprintf(stderr, "[gpu] begin_frame unavail (dr=%p tex=%p %dx%d)\n", dr, tex, w, h); s_warned = true; }
    if (tex) frct_metal_release(tex);
    if (dr)  frct_metal_release(dr);
    return false;
  }
  GrMtlTextureInfo mi;
  mi.fTexture = sk_cfp<GrMTLHandle>((GrMTLHandle)tex);   // adopt +1, released below
  GrBackendRenderTarget rt = GrBackendRenderTargets::MakeMtl(w, h, mi);
  sk_sp<SkSurface> surf = SkSurfaces::WrapBackendRenderTarget(
      s_dctx.get(), rt, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType,
      nullptr, nullptr);
  if (surf) {
    if (render) render(ctx, surf->getCanvas(), w, h);
    s_dctx->flushAndSubmit(surf.get());
  } else if (!s_warned) {
    fprintf(stderr, "[gpu] surface wrap failed\n");
  }
  frct_metal_present(s_metalCtx, dr);   // present + vsync
  frct_metal_release(dr);
  return true;
}

void frct_gpu_shutdown() {
  if (s_metalCtx) {
    frct_metal_destroy(s_metalCtx);
    s_metalCtx = nullptr;
  }
  s_dctx.reset();
}

}  // extern "C"