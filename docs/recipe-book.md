# Recipe Book — Design & Behaviour

In-game reference for **every transformation the simulation knows about**,
opened with **F2**. It is generated from the loaded tables and the balancing
constants, so it can never drift from the sim.

Implementation: `src/game/view/recipe_book.cpp` (overlay) +
`src/game/model/book_state.hpp` (progress marks).

## 1. Layout

```
┌ RECIPE BOOK ─────────────────────────────── F2 close  1-5 tabs  arrows scroll ┐
│ 1 Recipes   0/3 │  Hold the hammer (item 3) and right-click a cell of the shape│
│ 2 Machines  0/3 │  ┌───────────────────────────────────────────────────────┐   │
│ 3 Reactions 0/4 │  │ ● [pattern grid] -> [result] name xN          done     │   │
│ 4 Stacking  0/5 │  └───────────────────────────────────────────────────────┘   │
│ 5 Quantum   0/1 │  ...                                                       │
└────────────────────────────────────────────────────────────────────────────────┘
```

* **Tabbed**: sidebar categories (number keys 1-5, left/right arrows, or click),
  content pane on the right.
* Scroll the content with the wheel or up/down arrows; a slim indicator appears
  when a category overflows.
* While open the overlay is **modal**: clicks, wheel and keys drive the book only
  (the board is not pickable, the sim keeps running).

## 2. Categories

| # | Category | Source | Entries |
|---|---|---|---|
| 1 | Recipes | `assets/crafting.csv` | one mini-grid per shaped recipe, offsets drawn as authored, result + count |
| 2 | Machines | `assets/station_recipes.csv` | footprint with role colours (violet machine / green input / orange output / blue buffer), in → out, duration |
| 3 | Reactions | `assets/reactions.csv` | shift-click pair: A + B → result |
| 4 | Stacking | code-defined | Grow, Shrink, Vanish, Merge, Split (with the split chance from `Heat::ITEM_SPILL_*`); rows whose feature flag is off are greyed and labelled `off` |
| 5 | Quantum | code-defined | heat bar 0..255 with the fluctuation threshold marked, hold rate, roll chance |

Numbers in the text come from `heat_constants.hpp`, so tuning a knob changes the
book too. The Stacking tab also reflects the **feature flags**: automatic grow /
shrink / split are off by default (`Config::itemGrow/itemShrink/itemSplit`), and
the book draws those rows greyed with an `off` label instead of a progress mark —
so the manual always matches what the simulation will actually do.

## 3. Activation help ("how do I make this happen?")

Every tab carries instructions, generated from **two tables at the top of
`src/game/view/recipe_book.cpp`** — the single place to edit when a mechanic's
input changes or a flag is added/removed:

| Table | Scope | Example |
|---|---|---|
| `kCatHow[category]` | one line per category | Recipes: *"hold the hammer (item 3), right-click any cell of the shape"* |
| `kCatNote[category]` | extra context (tool source, caveats) | Recipes: *"the hammer itself: two wood react into one, or craft the L shape below"* |
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
layout changes without a window.

> `drawText` clips at 64 characters, so every line in the book must stay short —
> split long sentences into multiple `drawText` calls.
