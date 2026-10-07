# Items — the content table & progression

`assets/items.csv` (`id,name,R,G,B,icon,svgPath,sway`) is the single source of
truth for what exists on the board. Ids are an item's identity everywhere
(reactions, crafting, stations, the recipe book) — keep them stable, append new
items at the end.

## The set

| id | item | icon source | how you get it |
|---|---|---|---|
| 0 | wood | `Wood Stick.svg` | **found** (random world) |
| 1 | stone | `Pétanque Cochonnet.svg` | **found** |
| 2 | water | `Water Drop.svg` | **found** |
| 3 | **wand** | `CraftingWand.svg` | reaction `wood:1 + wood:1` — the crafting tool |
| 4 | coal | `Item Coal.svg` ← pack `Ore Coal` | reaction `wood:2 + stone:1` (charcoal) |
| 5 | ore | `Item Ore.svg` ← pack `Ore Copper` | reaction `stone:1 + water:1` (panning) |
| 6 | metal | `Item Metal.svg` ← pack `Iron` | **FURNACE**: `coal + ore` |
| 7 | plank | `Item Plank.svg` ← pack `Wood Planks` | craft 1×2: `wood + wood` |
| 8 | stick | `Item Stick.svg` ← pack `Baseball Bat` | craft 1×3: `wood ×3` |
| 9 | sand | `Item Sand.svg` ← pack `Sand` | reaction `stone:1 + stone:1` |
| 10 | brick | `Item Brick.svg` ← pack `Brick` | reaction `sand:1 + water:1` |
| 11 | glass | `Item Glass.svg` ← pack `Glass` | **KILN**: `sand + coal` |
| 12 | torch | `Item Torch.svg` ← pack `Torch` | reaction `stick:1 + coal:1` |
| 13 | screw | `Item Screw.svg` ← pack `Screw` | craft 1×2: `metal + metal` |
| 14 | ladder | `Item Ladder.svg` ← pack `Ladder` | reaction `stick:2 + plank:1` (3 cells) |
| 15 | gear | `Item Gear.svg` ← pack `Gear 2` | reaction `metal:1 + metal:1 + screw:1 + coal:1` (4 cells) |

The chain: **found basics → gathered (coal, ore, sand) → refined (metal, glass)
→ parts (plank, stick, screw) → built (torch, ladder, gear)**, with the wand as
the one tool the player has to bootstrap out of two sticks of wood.

## Rules the table has to obey

* **Only the ids marked `natural` (the last column) are grown by the random
  world** — `Config::numItems` is read from that column's contiguous prefix
  (`src/game/model/item_table.cpp`; `--num-items` overrides). Everything else
  must be made, which is what makes it a progression instead of a scavenger
  hunt. The prefix rule matters because generation rolls `id % numItems`: marking
  id 5 natural without 3 and 4 would make the wrong items spawn.
* **The crafting tool (id 3) must never be a crafting or station result** — a
  recipe for it costs the tool it makes, so it would be unobtainable. Its only
  source is the two-wood reaction (`docs/reactions.md` §2).
* **Every icon must carry inline fills.** Native renders item icons through
  `SkSVGDOM`, which ignores `<style>` blocks, so a CSS-classed pack SVG renders
  as a black blob; the wasm codegen (`tools/gen_wasm_svg_assets.py`) reads the
  same files and has the same rule. `assets/Item *.svg` are pack icons flattened
  with the icon-pack tooling (see the `fractory-item-icons` skill), which inlines
  the classes, drops opacity-0 backing rects and bakes `transform` into the path
  data. Never point `items.csv` at a raw pack SVG.
* **Two items must not share artwork.** `Wood Stick.svg` and the pack's
  `Stick.svg` are the *same drawing*, which is why the crafted stick is derived
  from `Baseball Bat` instead (a straight rod reads as a stick; the forked branch
  is already "wood"). Coal is the pack's grey `Ore Coal` recoloured to near-black
  so it does not read as stone.
* `sway` picks the animation: `static` = the whole icon rotates as one (solid
  objects — everything above), `perPath` = each `<path>` drifts on its own
  frequency (clumps; currently only stone).
* `R,G,B` tint the item's particles and the drag ghost
  (`GameView::elementColor`, which prefers this colour over the five built-in
  fallback colours).

## Checking the tables

`python3 tools/check_content.py` cross-checks all four tables: dangling ids, an
item that is neither natural nor produced by anything, reaction rules whose
ingredient set is already claimed by an earlier rule (the later one could never
fire), the crafting tool appearing as a crafting/station result, and two items
pointing at the same SVG. Run it after any content edit — it caught a plank
recipe that produced metal.
