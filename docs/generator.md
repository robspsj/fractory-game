# Random world generator

`GameModel::initRandomGrid` (`src/game/model/game_model.cpp`) builds the world the
game starts in: a root `GRID` of `GameModel::GRID` × `GameModel::GRID` cells
(5×5), whose cells may themselves become subgrids (2..7 per side, nested), items
or stay empty. Stations (machines) are scattered into it.

## Passes

1. **Subgrids / items / empties** — one roll per cell, walking the node array
   forward. A cell that becomes a subgrid appends its children *after* the
   current index, so the same walk descends into nested subgrids as it reaches
   them, up to `Config::gridLimit` subgrids. Roll shares:

   | share | cells | meaning |
   |---|---|---|
   | `gridChance` | 20 % | becomes a subgrid |
   | `stationChance` | 20 % | earmarked for a machine (left empty here) |
   | rest | 60 % | split evenly between `ITEM` and `EMPTY` |

   Once the subgrid budget is spent, the subgrid share falls back to items.
2. **Stations** — one anchor roll per cell that is still `EMPTY`; a hit plants a
   random recipe footprint from `assets/station_recipes.csv`. Every pass only
   writes cells that are still `EMPTY`, so a footprint claimed by pass 2 can
   never be re-rolled, and two footprints can never overlap.

The anchor of a footprint is its **top-left** cell — the same convention
`StationCell::place` uses for hand-placed machines — so a footprint only fits
inside its parent subgrid when the remaining rows/columns allow it. Recipes are
tried in a random rotation, which is why small machines still appear in subgrids
too small for a 2×3.

Density at the default `stationChance = 20` (500 k subgrids, ~11.5 M nodes):
~31 % of subgrids hold at least one machine and ~6 % of all cells belong to a
footprint. Set `stationChance = 0` for a machine-free world.

## Tunables

| flag | effect |
|---|---|
| `--station-chance N` | `Config::stationChance` (0..`100-gridChance`), machine density |
| `--no-stations` | same as `--station-chance 0` |
| `--grid-limit N` | cap on subgrids (board size) |

Data file: `assets/station_recipes.csv` — columns
`id,sizeR,sizeC,inputA,inputB,output,outputCount,duration,layout...`, where
`layout` is row-major over the footprint and `layout[0]` is the machine's own
tile (roles only apply to its `RESERVED` neighbours). Item ids come from
`assets/items.csv`. On the web build there is no filesystem, so CMake bakes the
same CSV into the binary (`tools/gen_wasm_interact_data.py` →
`generated/wasm_interact_data.h`, consumed as `Config::stationRecipesCsv`).

## Verifying

- `./build-native/fractory --tick 0` prints the node-type histogram
  (`EMPTY` / `ITEM` / `GRID` / `STATION` / `RESERVED`) for the generated world.
- `./build-native/fractory --headless out.ppm 1200 900` renders one world frame.
- `./test.sh` — CSV scenarios. `tests/station_recipes.csv` is the test harness's
  own table (its ids/layouts are what `tests/test_furnace.csv` expects); the
  recipes used for generated worlds are the ones in `assets/`.
