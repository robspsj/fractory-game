
#include "svg.h"

#include "svgshapes.h"
#include "bspanutil.h"


#include <algorithm>
#include <vector>
#include <memory>

//using namespace svg2b2d;



bool parseSVG(const void* bytes, const size_t sz, BLImage& outImage)
{
    svg2b2d::ByteSpan inChunk(bytes, sz);   // = svg2b2d::chunk_from_data_size(bytes, sz);
    
    // Create a new document
    svg2b2d::SVGDocument doc;

    // Load the document from the data
    doc.readFromData(inChunk);
    
    
    // Draw the document into a IRender
    outImage.create(doc.width(), doc.height(), BL_FORMAT_PRGB32);
    SVGRenderer ctx(outImage);
    // Clear to transparent — create() is uninitialized, and the vector only
    // paints the glyphs, leaving the background garbage (leaks as wrong colors).
    ctx.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx.setFillStyle(BLRgba32(0x00000000));
    ctx.fillAll();
    ctx.setCompOp(BL_COMP_OP_SRC_OVER);
    doc.draw(ctx);
    ctx.end();
    
    return true;
}

// Render the SVG as vector paths into a targetW x targetH image (crisp at any
// zoom — no bitmap upscaling), scaled to fit, preserving aspect, centered.
bool parseSVGscaled(const void* bytes, const size_t sz, int targetW, int targetH, BLImage& outImage)
{
    if (targetW <= 0 || targetH <= 0)
        return false;
    svg2b2d::ByteSpan inChunk(bytes, sz);
    svg2b2d::SVGDocument doc;
    if (!doc.readFromData(inChunk))
        return false;
    double dw = doc.width(), dh = doc.height();
    if (dw <= 0 || dh <= 0)
        return false;
    outImage.create(targetW, targetH, BL_FORMAT_PRGB32);
    SVGRenderer ctx(outImage);
    // IMPORTANT: create() does NOT zero the buffer. Clear to fully transparent
    // before drawing, otherwise pixels the vector never paints (the background)
    // keep stale/garbage alpha from a previous (different-size) rasterization —
    // those leak into the frame as crazy colors on resize/zoom.
    ctx.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx.setFillStyle(BLRgba32(0x00000000));
    ctx.fillAll();
    // Fit inside the target, preserving aspect ratio, centered.
    double scale = std::min((double)targetW / dw, (double)targetH / dh);
    double cx = (targetW - dw * scale) * 0.5;
    double cy = (targetH - dh * scale) * 0.5;
    ctx.setCompOp(BL_COMP_OP_SRC_OVER);
    ctx.scale(scale, scale);
    ctx.translate(cx / scale, cy / scale);
    doc.draw(ctx);
    ctx.end();
    return true;
}