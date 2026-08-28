#!/usr/bin/env python3
"""Write the rounded-color-square UI tiles as independent .svg files.

Each tile is a standalone SVG under assets/ui/ — bg_N.svg (grid container) and
cell_N.svg (inner item/empty cell), one colour family per grid size. The game
loads them at runtime, so editing a file and restarting is enough.

Corner-radius logic (so the background's corners match its cells' on screen):
  - A cell is rendered at width  cellW = gridW / (D + (D-1)*gap), D = cells/side.
  - Its corner radius on screen is  CELL_RX/64 * cellW.
  - The grid container is rendered at width gridW, so its SVG radius must be
    BG_RX = CELL_RX / (D + (D-1)*gap) to render the SAME pixel corner radius.
  Each variation i has D = i+1 cells per side, so BG_RX is computed per family.
  Only the fill is drawn (no dark stroke) so no dark "shadow" rim appears.

Regenerate after changing palette/radius/gap: python3 tools/gen_ui_assets.py
"""
import os

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, '..'))
uidir = os.path.join(root, 'assets', 'ui')
os.makedirs(uidir, exist_ok=True)

# Cell corner radius in SVG units (relative to cell width, so consistent at any
# scale in-game).
CELL_RX = 10.0
# Per-gap between adjacent cells (must match GameView::_gapRatio).
GAP = 0.0714

# Per-size colour families. Index = size-1 (sizes 1..5); clamped beyond.
PALETTE = [
    # (bg fill, cell fill)  size
    ('#6B7280', '#9CA3AF'),  # 1 gray
    ('#0D9488', '#2DD4BF'),  # 2 teal
    ('#D97706', '#FBBF24'),  # 3 amber
    ('#7C3AED', '#A78BFA'),  # 4 purple
    ('#2563EB', '#60A5FA'),  # 5 blue
]


def tile_svg(fill, rx):
    # preserveAspectRatio="none" stretches the tile to fill its container (its
    # non-square NDC rect maps to a square in pixels). The 65-pt square
    # (-0.5..64.5) reaches just past the cell edge, so the border sits at the
    # full edge with no inset "shadow". No stroke -> no dark rim between cells.
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" '
        'preserveAspectRatio="none" viewBox="0 0 64 64">'
        '<rect x="-0.5" y="-0.5" width="65" height="65" rx="%.1f" fill="%s"/>'
        '</svg>' % (rx, fill)
    )


def bg_radius(dim):
    """SVG corner radius for a grid container of `dim` cells per side, so its
    on-screen corner radius equals a contained cell's."""
    k = dim + (dim - 1) * GAP
    return CELL_RX / k


def main():
    for i, (bg, cell) in enumerate(PALETTE):
        dim = i + 1
        with open(os.path.join(uidir, 'bg_%d.svg' % i), 'w') as f:
            f.write(tile_svg(bg, bg_radius(dim)))
        with open(os.path.join(uidir, 'cell_%d.svg' % i), 'w') as f:
            f.write(tile_svg(cell, CELL_RX))
    print('wrote %d families to %s (cell rx=%.1f, bg rx per-size)' %
          (len(PALETTE), uidir, CELL_RX))


if __name__ == '__main__':
    main()