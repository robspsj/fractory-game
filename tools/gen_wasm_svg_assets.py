#!/usr/bin/env python3
"""Generate wasm_svg_assets.h — embedded SVG icon data for the WASM build.

The wasm Skia is built without expat → no SkSVGDOM, and the link runs with
-sFILESYSTEM=0 → assets/*.svg can't be read at runtime. This tool bakes the
icons referenced by assets/items.csv into a compact C++ header: per shape the
`d` path data (for <path> tags the raw string — the runtime parses it with
SkParsePath exactly like the native live-file loader, so rasterization is
bit-identical; for primitive shapes an equivalent d is synthesized), the fill
color, and fill/stroke style. All SVG markup overhead is stripped.

Parsing semantics mirror src/game/view/per_path_svg.cpp (the native live-file
loader): flat document-order shape scan, fill attr then style:fill then black,
transparent-fill shapes skipped (line/polyline fall back to stroke), viewBox
attr only (default 64x64).

Usage: python3 tools/gen_wasm_svg_assets.py <repo_root> <out_header.h>
"""
import os
import re
import sys

TAG_RE = re.compile(r"<(path|polygon|polyline|rect|circle|ellipse|line)\b([^>]*)(/?)>", re.I)
ATTR_RE = re.compile(r"""([a-zA-Z][-\w:]*)\s*=\s*("([^"]*)"|'([^']*)')""")

FILL = 0
STROKE = 1


def attrs(s):
    out = {}
    for m in ATTR_RE.finditer(s):
        out[m.group(1).lower()] = m.group(3) if m.group(3) is not None else (m.group(4) or "")
    return out


def tok(s):
    return [t for t in re.split(r"[, \t\r\n]+", s.strip()) if t]


def fnum(x):
    s = "%.3f" % x
    s = s.rstrip("0").rstrip(".")
    return s if s not in ("-0", "") else "0"


def parse_points(s):
    v = [float(x) for x in tok(s)]
    return [(v[i], v[i + 1]) for i in range(0, len(v) - 1, 2)]


def fattr(at, name, default=0.0):
    try:
        return float(at.get(name, "") or default)
    except ValueError:
        return default


def parse_color(s):
    """Mirror parseHexFill in per_path_svg.cpp: #rgb/#rrggbb hex; 'none' →
    transparent; anything else (named colors, missing) → opaque black."""
    if not s or s == "none":
        return (0, 0, 0, 0)
    hexs = s[1:] if s.startswith("#") else s
    try:
        if len(hexs) == 6:
            return (int(hexs[0:2], 16), int(hexs[2:4], 16), int(hexs[4:6], 16), 255)
        if len(hexs) == 3:
            return (int(hexs[0], 16) * 17, int(hexs[1], 16) * 17, int(hexs[2], 16) * 17, 255)
    except ValueError:
        pass
    return (0, 0, 0, 255)


def style_fill(at):
    m = re.search(r"fill\s*:\s*([^;]+)", at.get("style", ""))
    return m.group(1).strip() if m else ""


def rect_d(x, y, w, h, rx, ry):
    if rx <= 0 or ry <= 0:
        x2, y2 = x + w, y + h
        return "M%s %sL%s %sL%s %sL%s %sZ" % (fnum(x), fnum(y), fnum(x2), fnum(y),
                                              fnum(x2), fnum(y2), fnum(x), fnum(y2))
    rx, ry = min(rx, w / 2), min(ry, h / 2)
    x2, y2 = x + w, y + h
    a = "A%s %s 0 0 1 " % (fnum(rx), fnum(ry))
    return ("M%s %sL%s %s%s%s %sL%s %s%s%s %sL%s %s%s%s %sL%s %s%s%s %sZ" % (
        fnum(x + rx), fnum(y), fnum(x2 - rx), fnum(y),
        a, fnum(x2), fnum(y + ry), fnum(x2), fnum(y2 - ry),
        a, fnum(x2 - rx), fnum(y2), fnum(x + rx), fnum(y2),
        a, fnum(x), fnum(y2 - ry), fnum(x), fnum(y + ry),
        a, fnum(x + rx), fnum(y)))


def ellipse_d(cx, cy, rx, ry):
    return ("M%s %sA%s %s 0 0 1 %s %sA%s %s 0 0 1 %s %sZ" % (
        fnum(cx - rx), fnum(cy), fnum(rx), fnum(ry), fnum(cx + rx), fnum(cy),
        fnum(rx), fnum(ry), fnum(cx - rx), fnum(cy)))


def extract_icon(root, svg_path):
    with open(os.path.join(root, svg_path), encoding="utf-8") as f:
        content = f.read()
    vb_w, vb_h = 64.0, 64.0
    m = re.search(r'viewBox\s*=\s*"([^"]+)"', content)
    if m:
        v = tok(m.group(1))
        if len(v) == 4:
            vb_w, vb_h = float(v[2]), float(v[3])
    shapes = []
    for tag in TAG_RE.finditer(content):
        name = tag.group(1).lower()
        at = attrs(tag.group(2))
        fill_str = at.get("fill", "") or style_fill(at)
        d = None
        if name == "path":
            d = at.get("d")  # verbatim → identical to native PerPathSvg::Load
            if not d:
                continue
        elif name in ("polygon", "polyline"):
            pts = parse_points(at.get("points", ""))
            if len(pts) >= 2:
                d = "M%s %s" % (fnum(pts[0][0]), fnum(pts[0][1])) + \
                    "".join("L%s %s" % (fnum(x), fnum(y)) for x, y in pts[1:])
                if name == "polygon":
                    d += "Z"
        elif name == "rect":
            x, y = fattr(at, "x"), fattr(at, "y")
            w, h = fattr(at, "width"), fattr(at, "height")
            rx, ry = fattr(at, "rx"), fattr(at, "ry")
            if rx == 0:
                ry = 0.0
            if w > 0 and h > 0:
                d = rect_d(x, y, w, h, rx, ry)
        elif name == "circle":
            cx, cy, r = fattr(at, "cx"), fattr(at, "cy"), fattr(at, "r")
            if r > 0:
                d = ellipse_d(cx, cy, r, r)
        elif name == "ellipse":
            cx, cy = fattr(at, "cx"), fattr(at, "cy")
            rx, ry = fattr(at, "rx"), fattr(at, "ry")
            if rx > 0 and ry > 0:
                d = ellipse_d(cx, cy, rx, ry)
        elif name == "line":
            d = "M%s %sL%s %s" % (fnum(fattr(at, "x1")), fnum(fattr(at, "y1")),
                                  fnum(fattr(at, "x2")), fnum(fattr(at, "y2")))
        if d is None:
            continue
        style = STROKE if name in ("line", "polyline") else FILL
        sw = fattr(at, "stroke-width", 1.0) if style == STROKE else 0.0
        if style == STROKE and (not fill_str or fill_str == "none"):
            fill_str = at.get("stroke", "")
        r, g, b, a = parse_color(fill_str)
        if a == 0 and name in ("line", "polyline"):
            stroke = at.get("stroke", "")
            if stroke and stroke != "none":
                r, g, b, a = parse_color(stroke)
        if a == 0:
            continue
        if style == STROKE and sw <= 0:
            sw = 1.0
        shapes.append((d, r, g, b, style, sw))
    return vb_w, vb_h, shapes


def read_items_csv(root):
    rows = []
    with open(os.path.join(root, "assets", "items.csv"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            cols = [t.strip() for t in line.split(",")]
            if len(cols) < 6:
                continue
            try:
                item_id = int(cols[0])
                color = (float(cols[2]), float(cols[3]), float(cols[4]))
                icon = int(cols[5])
            except ValueError:
                continue
            svg = cols[6] if len(cols) > 6 else ""
            sway_raw = cols[7].lower() if len(cols) > 7 and cols[7] else "static"
            per_path = sway_raw in ("perpath", "per_path", "per-path", "1", "true", "yes", "sway")
            rows.append((item_id, color, icon, svg, per_path))
    return rows


def cstr(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip())
        sys.exit(1)
    root = os.path.abspath(sys.argv[1])
    out_path = sys.argv[2]

    rows = read_items_csv(root)
    icon_paths = []
    icon_idx = {}
    for _, _, _, svg, _ in rows:
        if svg and svg not in icon_idx:
            icon_idx[svg] = len(icon_paths)
            icon_paths.append(svg)

    out = []
    out.append("// Generated by tools/gen_wasm_svg_assets.py — DO NOT EDIT.")
    out.append("// Embedded icon geometry for the WASM build (no SkSVGDOM, no filesystem).")
    out.append("#pragma once")
    out.append("#include <cstdint>")
    out.append("#include <cstddef>")
    out.append("")
    out.append("namespace wasm_svg {")
    out.append("")
    out.append("enum ShapeStyle : uint8_t { kFill = 0, kStroke = 1 };")
    out.append("enum Sway : uint8_t { kStatic = 0, kPerPath = 1 };")
    out.append("struct Shape { const char* d; uint8_t r, g, b; ShapeStyle style; float strokeW; };")
    out.append("struct Icon { const char* path; float vbW, vbH; size_t count; const Shape* shapes; };")
    out.append("struct Item { int id; float r, g, b; int icon; int svgIdx; Sway sway; };")
    out.append("")

    total_shapes = 0
    icon_meta = []
    for n, svg in enumerate(icon_paths):
        vb_w, vb_h, shapes = extract_icon(root, svg)
        total_shapes += len(shapes)
        icon_meta.append((svg, vb_w, vb_h, shapes))
        out.append("inline constexpr Shape kIcon%d[] = {" % n)
        for d, r, g, b, style, sw in shapes:
            out.append("    {%s, %d, %d, %d, %s, %sf}," % (cstr(d), r, g, b,
                       "kFill" if style == FILL else "kStroke", repr(round(sw, 3))))
        out.append("};")
    out.append("")
    out.append("inline constexpr Icon kIcons[] = {")
    for n, (svg, vb_w, vb_h, shapes) in enumerate(icon_meta):
        out.append("    {%s, %sf, %sf, %d, kIcon%d}," % (cstr(svg), repr(vb_w), repr(vb_h), len(shapes), n))
    out.append("};")
    out.append("inline constexpr size_t kIconCount = %d;" % len(icon_paths))
    out.append("")
    out.append("inline constexpr Item kItems[] = {")
    for item_id, color, icon, svg, per_path in rows:
        out.append("    {%d, %sf, %sf, %sf, %d, %d, %s}," % (
            item_id, repr(color[0]), repr(color[1]), repr(color[2]), icon,
            icon_idx.get(svg, -1), "kPerPath" if per_path else "kStatic"))
    out.append("};")
    out.append("inline constexpr size_t kItemCount = %d;" % len(rows))
    out.append("")
    out.append("}  // namespace wasm_svg")

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print("gen_wasm_svg_assets: %d items, %d icons, %d shapes -> %s"
          % (len(rows), len(icon_paths), total_shapes, out_path))


if __name__ == "__main__":
    main()
