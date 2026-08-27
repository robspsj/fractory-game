// Minimal Metal glue for the native GPU present path.
//
// Compiled as Objective-C++ (ARC). Hands an MTLDevice+MTLCommandQueue to
// main.cpp (which builds Skia's GrMtlBackendContext), then per frame returns
// the CAMetalLayer drawable's texture for Skia to render into, and presents it
// with an ordered command buffer (vsync via the drawable). All handles cross to
// C as retained CFTypeRefs (CFBridgingRetain) — main.cpp owns/releases them.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#import <SDL3/SDL.h>
#import <SDL3/SDL_metal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FRCTMetalContext {
  CFTypeRef device;   // id<MTLDevice> as retained CFTypeRef (+1)
  CFTypeRef queue;    // id<MTLCommandQueue> (+1)
  CFTypeRef layer;    // CAMetalLayer (+1)
  SDL_MetalView view; // not CF-managed; freed in destroy
} FRCTMetalContext;

void* frct_metal_create(void* sdlWindow, void** out_device, void** out_queue, int vsync) {
  @autoreleasepool {
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    if (!dev) return nullptr;
    id<MTLCommandQueue> q = [dev newCommandQueue];
    if (!q) return nullptr;
    SDL_Window* win = (SDL_Window*)sdlWindow;
    SDL_MetalView view = SDL_Metal_CreateView(win);
    if (!view) return nullptr;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(view);
    layer.device = dev;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    // V-sync control: when vsync is OFF, disable the layer's display sync so the
    // present drawable is NOT throttled to the display refresh (unlocked FPS).
    layer.displaySyncEnabled = vsync ? YES : NO;
    // Match the layer's backing store to the window size (backing pixels).
    int ww = 0, hh = 0;
    SDL_GetWindowSizeInPixels(win, &ww, &hh);
    if (ww > 0 && hh > 0)
      layer.drawableSize = CGSizeMake(ww, hh);
    FRCTMetalContext* c = (FRCTMetalContext*)calloc(1, sizeof(*c));
    c->device = CFBridgingRetain(dev);
    c->queue = CFBridgingRetain(q);
    c->layer = CFBridgingRetain(layer);
    c->view = view;
    if (out_device) *out_device = (void*)CFBridgingRetain(dev);
    if (out_queue) *out_queue = (void*)CFBridgingRetain(q);
    return c;
  }
}

// Returns a retained (CFTypeRef) drawable to present later, and writes the
// drawable texture (retained CFTypeRef) + its size/format for Skia to wrap.
void* frct_metal_begin_frame(void* ctx, void** out_texture, int* out_w, int* out_h,
                             unsigned* out_format) {
  @autoreleasepool {
    FRCTMetalContext* c = (FRCTMetalContext*)ctx;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)c->layer;
    id<CAMetalDrawable> dr = [layer nextDrawable];
    if (!dr) return nullptr;
    id<MTLTexture> tex = dr.texture;
    if (out_texture) *out_texture = (void*)CFBridgingRetain(tex);
    if (out_w) *out_w = (int)tex.width;
    if (out_h) *out_h = (int)tex.height;
    if (out_format) *out_format = (unsigned)tex.pixelFormat;
    return (void*)CFBridgingRetain(dr);
  }
}

// Present the given drawable. The command buffer is created on the SAME queue
// Skia renders on, so it is ordered after this frame's GPU work. Drawable
// presentation is v-synced with the display refresh.
void frct_metal_present(void* ctx, void* drawableRef) {
  @autoreleasepool {
    FRCTMetalContext* c = (FRCTMetalContext*)ctx;
    id<CAMetalDrawable> dr = (__bridge id<CAMetalDrawable>)drawableRef;
    id<MTLCommandQueue> q = (__bridge id<MTLCommandQueue>)c->queue;
    id<MTLCommandBuffer> cb = [q commandBuffer];
    [cb presentDrawable:dr];
    [cb commit];
  }
}

void frct_metal_release(void* ref) {
  if (ref) CFRelease((CFTypeRef)ref);
}

void frct_metal_destroy(void* ctx) {
  FRCTMetalContext* c = (FRCTMetalContext*)ctx;
  if (!c) return;
  @autoreleasepool {
    SDL_Metal_DestroyView(c->view);
  }
  if (c->device) CFRelease(c->device);
  if (c->queue) CFRelease(c->queue);
  if (c->layer) CFRelease(c->layer);
  free(c);
}

#ifdef __cplusplus
}  // extern "C"
#endif