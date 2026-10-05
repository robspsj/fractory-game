# Recipe Book — Design & Behaviour

In-game reference for **every transformation the simulation knows about**,
opened with **F2**. It is generated from the loaded tables and the balancing
constants, so it can never drift from the sim.

Implementation: `src/game/view/recipe_book.cpp` (overlay) +
`src/game/model/book_state.hpp` (progress marks).

## 1. Layout (responsive)

```
wide window                              narrow / portrait window
┌ RECIPE BOOK ────── F2 close … green row = done ┐   ┌ RECIPE BOOK ──────────┐
│ 1 Recipes   0/3 │  How: hold the wand  …       │   │ 1 Recipes 0/3  2 Mach…│  <- tab strip
│ 2 Machines  0/2 │  ┌────────────────────────┐   │   ├───────────────────────┤
│ 3 Reactions 0/8 │  │ ● [pattern] -> [result]│ ▓ │   │ How: hold the wand  … │
│ 4 Stacking  0/5 │  └────────────────────────┘ ▓ │   │ ● [pattern]           │
│ 5 Quantum   0/1 │  ...                     ░ ▓ │   │   -> [result]         │
└─────────────────────────────────────────────────┘   └───────────────────────┘
   sidebar tabs + right pane, scrollbar at the edge     top tabs, full-width rows
```

Everything is derived from the window size (`Metrics`/`layoutFor` at the top of
`recipe_book.cpp`), so the book works from a small web canvas to a 4K window:

* `ui` scales geometry (cell, icon, margins, tabs); `o` is a gentler scale for
  structural offsets; **text is floored** (`text`/`small`) so it stays legible —
  overflow is absorbed by wrapping and the scroll view rather than shrinking type.
  Current type: `text = clamp(3.5*ui, 2.7, 4.1)`, `small = clamp(2.84*ui, 2.3, 3.3)`,
  `title = 1.34*text`. Line spacing (`gap`) and every container that holds text
  (title band, tab strip, sidebar rows, sidebar width, scroll step) is derived
  from `textHeight()`/`textWidth()` — **never** hard-coded — so changing those
  three scales reflows the panel instead of overlapping it.
* **Long text word-wraps** to the pane (`wrapText` + `drawLines`, used for every
  intro, help line, row description and machine label). Rows measure their
  wrapped line count *before* drawing so row heights always fit the type; this
  also dodges `drawText`'s 64-char clip.
* **Tabs move to a top strip** when a left column would leave the content narrower
  than ~400px (small windows, portrait/phone aspect ratios), and their labels
  degrade (`1 Recipes 0/3` → `1 Recipes` → `1`) rather than overrun the pill.
  `layoutFor` decides once per draw and `click()` hit-tests the same geometry, so
  both stay in sync.
* **Rows re-flow when the pane is tight** rather than clipping: a recipe's result
  and a machine's text block drop *below* the shape instead of sitting beside it.
* The title band drops items as room runs out: full hint → keys only → nothing,
  with the legend always laid out *relative to* the hint (never both right-aligned
  to the same edge, which overlapped them).

### Scroll view

The content pane is clipped and scrolled by `_scroll`; the wheel and up/down
arrows move it, and when the content overflows a real **scrollbar** appears at the
right edge and can be **dragged** (`click()` grabs it, `dragTo()` follows the
mouse, `endDrag()` releases — `Game::update`/`mouseUp` forward those while the
book is open). The bar is only drawn when it is needed, and its geometry is
measured during `draw()` so hit-testing uses the very same numbers.

While open the overlay is **modal**: clicks, motion, wheel and keys drive the book
only (the board is not pickable, the sim keeps running).

## 2. Categories

| # | Category | Source | Entries |
|---|---|---|---|
| 1 | Recipes | `assets/crafting.csv` | one mini-grid per shaped recipe, offsets drawn as authored, result + count |
| 2 | Machines | `assets/station_recipes.csv` | footprint with role colours (violet machine / green input / orange output / blue buffer), in → out, duration |
| 3 | Reactions | `assets/reactions.csv` | gap click pair: A + B → result |
| 4 | Stacking | code-defined | Grow, Shrink, Vanish, Merge, Split (with the split chance from `Heat::ITEM_SPILL_*`); rows whose feature flag is off are greyed and labelled `off` |
| 5 | Quantum | code-defined | heat bar 0..255 with the fluctuation threshold marked, hold rate, roll chance |

Numbers in the text come from `heat_constants.hpp`, so tuning a knob changes the
book too. The Stacking tab also reflects the **feature flags**: automatic grow /
shrink / split are off by default (`Config::itemGrow/itemShrink/itemSplit`), and
the book draws those rows greyed with an `off` label instead of a progress mark —
so the manual always matches what the simulation will actually do.

## 3. Activation help ("how do I make this happen?")

Every tab carries instructions, generated from **three tables at the top of
`src/game/view/recipe_book.cpp`** — the single place to edit when a mechanic's
input changes or a flag is added/removed:

| Table | Scope | Example |
|---|---|---|
| `kCatHow[category]` | one line per category | Recipes: *"hold the wand (item 3), right-click any cell of the shape"* |
| `kCatNote[category]` | extra context (tool source, caveats) | Recipes: *"the wand itself is never crafted: two wood react into one (Reactions)"* |
| `kPhysicsHow[BookPhysics]` | per-entry, for the code-defined rows | Split: *"automatic while --item-split / --item-dynamics is on"* |

Drawn under each section intro (`drawCatHelp`) and inside each Stacking row, so
the manual always states how a mechanic is triggered — including the CLI flag for
the ones that are off by default.

> When you iterate on a mechanic: change the input handling, then update the
> matching string(s) in those tables. If a new mechanic gets a flag, put the flag
> name in its `How:` line so the book doubles as the flag reference.

## 4. Progress marks

Everything is listed from the start (the book doubles as the manual); entries the
player has **performed** are marked — green row + dot + `done` label. The sidebar
shows `performed/total` per category.

`BookState` (`src/game/model/book_state.hpp`) holds one flag per table row plus a
bitmask for the code-defined physics entries:

```
BookCategory: Recipes, Machines, Reactions, Stacking, Quantum
BookPhysics:  Grow, Shrink, Destroy, Merge, Split, Quantum
```

Flags are set **at the moment the transformation happens**, from the model:
craft commit (`tryCraftWithDrag`), reaction (`ItemCell::interact`), machine
output (`StationCell::tick`), muts (GROW/SHRINK/DESTROY), spill apply (Split),
merge (`drop`), quantum birth (`quantumFluctuation`). Tests can assert them with
the `expect_performed,<category>,<id>,<0|1>` harness step.

## 5. Extending it

* A new table row appears automatically (the book iterates the loaded vectors).
* A new *category* needs: a `BookCategory` value, a name in `kCatName`, a
  `catTotal`/`catDone` case, and a render block in the `switch`.
* **Clickable entries are the intended next step**: entries are already addressed
  by `(category, row index)` — the same id space `BookState::mark()` uses — so a
  click can map to a recipe/rule without any new bookkeeping.

## 6. Visual checks

`--book [category]` renders the overlay into a headless frame, e.g.

```
./build-native/fractory --headless /tmp/book.ppm 1150 720 --empty-grid --book 4
```

(0=Recipes 1=Machines 2=Reactions 3=Stacking 4=Quantum). Handy for eyeballing
layout changes without a window. When touching the layout, render a spread of
window shapes — `1150x720` (reference), `700x900` (portrait → tab strip + stacked
rows), `560x360` (small → scrollbar), `1200x420` (wide/short):

```
for wh in 1150x720 700x900 560x360 1200x420; do
  ./build-native/fractory --headless /tmp/book_$wh.ppm ${wh%x*} ${wh#*x} --empty-grid --book 3
done
```

> `drawText` clips at 64 characters, so every line in the book must stay short —
> split long sentences into multiple `drawText` calls.
