#!/usr/bin/env python3
"""Generate 8x8 bitmap font data for ASCII 32-126, matching existing style."""

import sys

def line(pat):
    """Convert a pattern string like '..XXXX..' to a hex byte (MSB first)."""
    pat = pat.replace('.', '0').replace('X', '1')
    return int(pat, 2)

def glyph(*rows):
    """Take 8 row-pattern strings, return list of 8 hex bytes."""
    return [line(r) for r in rows]

# fmt: off
# Each glyph is 8 rows of 8 pixels. '.' = off, 'X' = on.
# Ordered by ASCII code from 32 (space) to 126 (tilde).
glyphs = []
# 32 space
glyphs.append(glyph(
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
))
# 33 !
glyphs.append(glyph(
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "........",
    "...XX...",
    "........",
))
# 34 "
glyphs.append(glyph(
    "..X..X..",
    "..X..X..",
    "..X..X..",
    "........",
    "........",
    "........",
    "........",
    "........",
))
# 35 #
glyphs.append(glyph(
    "..X..X..",
    "..X..X..",
    ".XXXXXX.",
    "..X..X..",
    ".XXXXXX.",
    "..X..X..",
    "..X..X..",
    "........",
))
# 36 $
glyphs.append(glyph(
    "...XX...",
    ".XXXXXX.",
    "XX......",
    ".XXXX...",
    ".....XX.",
    ".XXXXXX.",
    "...XX...",
    "........",
))
# 37 %
glyphs.append(glyph(
    "XX...X..",
    "XX..X...",
    "...X....",
    "..X.....",
    ".X...XX.",
    "X...XX..",
    "........",
    "........",
))
# 38 &
glyphs.append(glyph(
    "..XX....",
    ".X..X...",
    ".X..X...",
    "..XX....",
    ".X..X.XX",
    ".X...X..",
    "..XX.X..",
    "........",
))
# 39 '
glyphs.append(glyph(
    "...XX...",
    "...XX...",
    "...XX...",
    "........",
    "........",
    "........",
    "........",
    "........",
))
# 40 (
glyphs.append(glyph(
    "....X...",
    "...X....",
    "..X.....",
    "..X.....",
    "..X.....",
    "...X....",
    "....X...",
    "........",
))
# 41 )
glyphs.append(glyph(
    "...X....",
    "....X...",
    ".....X..",
    ".....X..",
    ".....X..",
    "....X...",
    "...X....",
    "........",
))
# 42 *
glyphs.append(glyph(
    "........",
    "...XX...",
    ".XX.XX..",
    "..XXX...",
    ".XX.XX..",
    "...XX...",
    "........",
    "........",
))
# 43 +
glyphs.append(glyph(
    "........",
    "...XX...",
    "...XX...",
    ".XXXXXX.",
    "...XX...",
    "...XX...",
    "........",
    "........",
))
# 44 ,
glyphs.append(glyph(
    "........",
    "........",
    "........",
    "........",
    "...XX...",
    "...XX...",
    "..XX....",
    "........",
))
# 45 -
glyphs.append(glyph(
    "........",
    "........",
    "........",
    ".XXXXXX.",
    "........",
    "........",
    "........",
    "........",
))
# 46 .
glyphs.append(glyph(
    "........",
    "........",
    "........",
    "........",
    "........",
    "...XX...",
    "...XX...",
    "........",
))
# 47 /
glyphs.append(glyph(
    ".....X..",
    "....X...",
    "...X....",
    "..X.....",
    ".X......",
    "X.......",
    "........",
    "........",
))
# 48 0
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 49 1
glyphs.append(glyph(
    "...XX...",
    "..XXX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    ".XXXXXX.",
    "........",
))
# 50 2
glyphs.append(glyph(
    ".XXXXXX.",
    ".....XX.",
    "....XX..",
    "...XX...",
    "..XX....",
    ".XX.....",
    ".XXXXXX.",
    "........",
))
# 51 3
glyphs.append(glyph(
    ".XXXXXX.",
    ".....XX.",
    "....XX..",
    "...XXX..",
    ".....XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 52 4
glyphs.append(glyph(
    "....XX..",
    "...XXX..",
    "..XXXX..",
    ".XX.XX..",
    "XXXXXXX.",
    "....XX..",
    "....XX..",
    "........",
))
# 53 5
glyphs.append(glyph(
    ".XXXXXX.",
    ".XX.....",
    ".XXXXX..",
    ".....XX.",
    ".....XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 54 6
glyphs.append(glyph(
    "..XXXX..",
    ".XX.....",
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 55 7
glyphs.append(glyph(
    ".XXXXXX.",
    ".....XX.",
    "....XX..",
    "...XX...",
    "..XX....",
    "..XX....",
    "..XX....",
    "........",
))
# 56 8
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 57 9
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXXX.",
    ".....XX.",
    ".....XX.",
    "..XXXX..",
    "........",
))
# 58 :
glyphs.append(glyph(
    "........",
    "...XX...",
    "...XX...",
    "........",
    "........",
    "...XX...",
    "...XX...",
    "........",
))
# 59 ;
glyphs.append(glyph(
    "........",
    "...XX...",
    "...XX...",
    "........",
    "........",
    "...XX...",
    "...XX...",
    "..XX....",
))
# 60 <
glyphs.append(glyph(
    "....XX..",
    "...XX...",
    "..XX....",
    ".XX.....",
    "..XX....",
    "...XX...",
    "....XX..",
    "........",
))
# 61 =
glyphs.append(glyph(
    "........",
    "........",
    ".XXXXXX.",
    "........",
    ".XXXXXX.",
    "........",
    "........",
    "........",
))
# 62 >
glyphs.append(glyph(
    "..XX....",
    "...XX...",
    "....XX..",
    ".....X..",
    "....XX..",
    "...XX...",
    "..XX....",
    "........",
))
# 63 ?
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".....XX.",
    "....XX..",
    "...XX...",
    "........",
    "...XX...",
    "........",
))
# 64 @
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX.XXX.",
    ".XX.XXX.",
    ".XX.XX..",
    ".XX.....",
    "..XXXX..",
    "........",
))
# 65 A
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 66 B
glyphs.append(glyph(
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXX..",
    "........",
))
# 67 C
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 68 D
glyphs.append(glyph(
    ".XXXX...",
    ".XX.XX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX.XX..",
    ".XXXX...",
    "........",
))
# 69 E
glyphs.append(glyph(
    ".XXXXXX.",
    ".XX.....",
    ".XX.....",
    ".XXXXX..",
    ".XX.....",
    ".XX.....",
    ".XXXXXX.",
    "........",
))
# 70 F
glyphs.append(glyph(
    ".XXXXXX.",
    ".XX.....",
    ".XX.....",
    ".XXXXX..",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    "........",
))
# 71 G
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX.....",
    ".XX.XXX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 72 H
glyphs.append(glyph(
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 73 I
glyphs.append(glyph(
    ".XXXXXX.",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    ".XXXXXX.",
    "........",
))
# 74 J
glyphs.append(glyph(
    "..XXXXX.",
    "....XX..",
    "....XX..",
    "....XX..",
    "....XX..",
    ".XX.XX..",
    "..XXX...",
    "........",
))
# 75 K
glyphs.append(glyph(
    ".XX..XX.",
    ".XX.XX..",
    ".XXXX...",
    ".XXX....",
    ".XXXX...",
    ".XX.XX..",
    ".XX..XX.",
    "........",
))
# 76 L
glyphs.append(glyph(
    ".XX.....",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    ".XXXXXX.",
    "........",
))
# 77 M
glyphs.append(glyph(
    ".XX..XX.",
    ".XXXXXX.",
    ".XXXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 78 N
glyphs.append(glyph(
    ".XX..XX.",
    ".XXX.XX.",
    ".XXXXXX.",
    ".XXXXXX.",
    ".XX.XXX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 79 O
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 80 P
glyphs.append(glyph(
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXX..",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    "........",
))
# 81 Q
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX.XX..",
    "..XXXX.X",
    "........",
))
# 82 R
glyphs.append(glyph(
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXX..",
    ".XXXX...",
    ".XX.XX..",
    ".XX..XX.",
    "........",
))
# 83 S
glyphs.append(glyph(
    "..XXXX..",
    ".XX..XX.",
    ".XX.....",
    "..XXXX..",
    ".....XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 84 T
glyphs.append(glyph(
    ".XXXXXX.",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "........",
))
# 85 U
glyphs.append(glyph(
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 86 V
glyphs.append(glyph(
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "...XX...",
    "........",
))
# 87 W
glyphs.append(glyph(
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXXX.",
    ".XXXXXX.",
    ".XX..XX.",
    "........",
))
# 88 X
glyphs.append(glyph(
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "...XX...",
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 89 Y
glyphs.append(glyph(
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "........",
))
# 90 Z
glyphs.append(glyph(
    ".XXXXXX.",
    ".....XX.",
    "....XX..",
    "...XX...",
    "..XX....",
    ".XX.....",
    ".XXXXXX.",
    "........",
))
# 91 [
glyphs.append(glyph(
    "..XXXX..",
    "..XX....",
    "..XX....",
    "..XX....",
    "..XX....",
    "..XX....",
    "..XXXX..",
    "........",
))
# 92 backslash
glyphs.append(glyph(
    ".X......",
    "..X.....",
    "...X....",
    "....X...",
    ".....X..",
    "......X.",
    "........",
    "........",
))
# 93 ]
glyphs.append(glyph(
    "..XXXX..",
    "....XX..",
    "....XX..",
    "....XX..",
    "....XX..",
    "....XX..",
    "..XXXX..",
    "........",
))
# 94 ^
glyphs.append(glyph(
    "...XX...",
    "..XXXX..",
    ".XX..XX.",
    "........",
    "........",
    "........",
    "........",
    "........",
))
# 95 _
glyphs.append(glyph(
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    ".XXXXXXX",
    "........",
))
# 96 `
glyphs.append(glyph(
    "..XX....",
    "...XX...",
    "....XX..",
    "........",
    "........",
    "........",
    "........",
    "........",
))
# 97 a
glyphs.append(glyph(
    "........",
    "........",
    "..XXXX..",
    ".....XX.",
    "..XXXXX.",
    ".XX..XX.",
    "..XXXXX.",
    "........",
))
# 98 b
glyphs.append(glyph(
    ".XX.....",
    ".XX.....",
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXX..",
    "........",
))
# 99 c
glyphs.append(glyph(
    "........",
    "........",
    "..XXXX..",
    ".XX..XX.",
    ".XX.....",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 100 d
glyphs.append(glyph(
    ".....XX.",
    ".....XX.",
    "..XXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXXX.",
    "........",
))
# 101 e
glyphs.append(glyph(
    "........",
    "........",
    "..XXXX..",
    ".XX..XX.",
    ".XXXXXX.",
    ".XX.....",
    "..XXXX..",
    "........",
))
# 102 f
glyphs.append(glyph(
    "...XXX..",
    "..XX....",
    "..XX....",
    ".XXXXX..",
    "..XX....",
    "..XX....",
    "..XX....",
    "........",
))
# 103 g
glyphs.append(glyph(
    "........",
    "........",
    "..XXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXXX.",
    ".....XX.",
    "..XXXX..",
))
# 104 h
glyphs.append(glyph(
    ".XX.....",
    ".XX.....",
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 105 i
glyphs.append(glyph(
    "...XX...",
    "........",
    "..XXX...",
    "...XX...",
    "...XX...",
    "...XX...",
    ".XXXXXX.",
    "........",
))
# 106 j
glyphs.append(glyph(
    "....XX..",
    "........",
    "...XXX..",
    "....XX..",
    "....XX..",
    "....XX..",
    ".XX.XX..",
    "..XXX...",
))
# 107 k
glyphs.append(glyph(
    ".XX.....",
    ".XX.....",
    ".XX..XX.",
    ".XX.XX..",
    ".XXXX...",
    ".XX.XX..",
    ".XX..XX.",
    "........",
))
# 108 l
glyphs.append(glyph(
    "..XXX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    ".XXXXXX.",
    "........",
))
# 109 m
glyphs.append(glyph(
    "........",
    "........",
    ".XXX.XX.",
    ".XXXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 110 n
glyphs.append(glyph(
    "........",
    "........",
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "........",
))
# 111 o
glyphs.append(glyph(
    "........",
    "........",
    "..XXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "........",
))
# 112 p
glyphs.append(glyph(
    "........",
    "........",
    ".XXXXX..",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXX..",
    ".XX.....",
    ".XX.....",
))
# 113 q
glyphs.append(glyph(
    "........",
    "........",
    "..XXXXX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXXX.",
    ".....XX.",
    ".....XX.",
))
# 114 r
glyphs.append(glyph(
    "........",
    "........",
    ".XX.XXX.",
    ".XXX....",
    ".XX.....",
    ".XX.....",
    ".XX.....",
    "........",
))
# 115 s
glyphs.append(glyph(
    "........",
    "........",
    "..XXXX..",
    ".XX.....",
    "..XXXX..",
    ".....XX.",
    ".XXXXX..",
    "........",
))
# 116 t
glyphs.append(glyph(
    "..XX....",
    "..XX....",
    ".XXXXX..",
    "..XX....",
    "..XX....",
    "..XX....",
    "...XXX..",
    "........",
))
# 117 u
glyphs.append(glyph(
    "........",
    "........",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXXX.",
    "........",
))
# 118 v
glyphs.append(glyph(
    "........",
    "........",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXX..",
    "...XX...",
    "........",
))
# 119 w
glyphs.append(glyph(
    "........",
    "........",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    ".XXXXXX.",
    ".XX..XX.",
    "........",
))
# 120 x
glyphs.append(glyph(
    "........",
    "........",
    ".XX..XX.",
    "..XXXX..",
    "...XX...",
    "..XXXX..",
    ".XX..XX.",
    "........",
))
# 121 y
glyphs.append(glyph(
    "........",
    "........",
    ".XX..XX.",
    ".XX..XX.",
    ".XX..XX.",
    "..XXXXX.",
    ".....XX.",
    "..XXXX..",
))
# 122 z
glyphs.append(glyph(
    "........",
    "........",
    ".XXXXXX.",
    "....XX..",
    "...XX...",
    "..XX....",
    ".XXXXXX.",
    "........",
))
# 123 {
glyphs.append(glyph(
    "....XXX.",
    "...XX...",
    "...XX...",
    ".XXX....",
    "...XX...",
    "...XX...",
    "....XXX.",
    "........",
))
# 124 |
glyphs.append(glyph(
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "...XX...",
    "........",
))
# 125 }
glyphs.append(glyph(
    ".XXX....",
    "...XX...",
    "...XX...",
    "....XXX.",
    "...XX...",
    "...XX...",
    ".XXX....",
    "........",
))
# 126 ~
glyphs.append(glyph(
    "........",
    ".XX.....",
    ".XXXX.XX",
    "....XX..",
    "........",
    "........",
    "........",
    "........",
))
# fmt: on

assert len(glyphs) == 95  # ASCII 32-126

# Output as C array
print(f"// {len(glyphs)} glyphs (ASCII 32-126), each {len(glyphs[0])} bytes")
print("static const unsigned char fontBits[NUM_GLYPH * FONT_H] = {")
for i, g in enumerate(glyphs):
    hexes = ", ".join(f"0x{b:02X}" for b in g)
    comma = "," if i < len(glyphs) - 1 else " "
    c = chr(i + 32)
    esc = {"\\": "\\\\", "'": "\\'", '"': '\\"'}
    c_str = esc.get(c, c)
    print(f"    {hexes}{comma} // {i:3d} '{c_str}' (ASCII {i+32})")
print("};")
