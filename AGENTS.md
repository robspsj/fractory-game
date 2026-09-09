# AGENTS.md

Ask clarifying questions when in doubt.

After confirming a change works and the app compiles cleanly, commit the changes with a descriptive message.

## Critical context (do not infer from docs)

- Despite the repo name, the renderer is **prebuilt Skia + SDL3**, not Blend2D. `README.md` and `docs/architecture.md` describe the retired Blend2D/OpenGL renderer — trust `CMakeLists.txt` and `src/` as source of truth.
- Skia is a **sibling checkout** at `../skia`, prebuilt via GN into `out/native/` (libskia.a, libsvg.a, …) and `out/wasm/` (libskia.wasm.a). CMake just links those archives; a fresh clone without `../skia` built will fail at link time, not compile time.
- C++20 (`CMAKELists` sets it; `.clangd`'s `--std=c++17` and blend2d include paths are stale leftovers). Generator defaults to Ninja, build type to Release.
- Only two build dirs are current: `build-native/` (native) and `build-wasm2/` (web). `build/`, `build-wasm/`, `cmake-build-debug/` are stale.

## Commands

- Native build+run: `./run.sh [game flags]` → `build-native/fractory`
- Tests: `./test.sh` — headless CSV model suite, runs `build-native/fractory --test tests/*.csv`. Single test: `./build-native/fractory --test tests/test_furnace.csv`.
  - Some `test_*.csv` scenario tests fail on pre-existing stale model expectations — a FAIL is not necessarily your regression; check whether it fails on HEAD too.
  - Non-`test_*.csv` files in `tests/` (reactions/spawns/station_recipes) are data inputs, not scenarios.
- Web build+serve: `./serve.sh [port]` → `build-wasm2/` (emcmake; script forces Homebrew `python@3.14` onto PATH because Emscripten otherwise picks Xcode's 3.9). If `../skia/out/wasm` was rebuilt (args.gn has `skia_enable_ganesh = true`), re-run `ninja -C ../skia/out/wasm skia` before linking.
- Web visual check without a real browser (headless Chrome uses SwiftShader software GL — fine for correctness, not fps; the isolated `--user-data-dir` is required or it hangs on an existing profile): `"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --user-data-dir=/tmp/chrome_hl_iso --enable-unsafe-swiftshader --window-size=900,650 --timeout=20000 --screenshot=/tmp/w.png http://127.0.0.1:8080/`. Force the CPU present path with `?cpu=1` in the URL.
- Deploy: `tools/deploy_itch.sh [osx|html5 ...] [--dry-run]` (requires `butler login`).
- Verification without a window: `--headless out.ppm [W H]` renders one deterministic frame offscreen; `--bench [W H] [frames]` for render perf; `--tick <N>` (+ `FRACTORY_TICK_THREADS`) for model-tick micro-bench; `--cpu` forces the CPU present path (default is Metal).

## Architecture

- `src/game/model/` is render-free game logic (cells: EMPTY/ITEM/GRID/STATION/RESERVED; flat `std::vector<Cell>` node array, root = index 0) and is what the CSV `--test` harness drives. Keep it free of Skia/SDL includes.
- Spatial neighbour lookups go through `GameModel::_adj`: a `std::vector<Adjacency>` parallel to `_nodes` holding, per leaf cell, the deepest world-space neighbour in each of the 4 `ADJ_*` directions (may cross subgrid boundaries; -1 off-world). Rebuilt by `rebuildAdjacency()` in `init()` and `setFullState()` — **any new topology mutator (adding/removing nodes, reparenting grids) must call it**, content-only mutations (drop/merge/spill/station) do not.
- `src/game/view/` is split per renderer (cell_renderer, icon_renderer, badge_renderer, background_renderer, camera_controller, picking_engine, …); `game.cpp` is the controller.
- Native presentation: Metal GPU path (`src/metal_gpu.cpp` + `src/metal_glue.mm`, compiled with `-fobjc-arc`) or CPU fallback. WASM presents via **Ganesh/WebGL2** (`src/webgl_gpu.cpp`; the `../skia` wasm build has `skia_enable_ganesh = true` and links with `-sMAX_WEBGL_VERSION=2`), with the old CPU-raster + `putImageData` path as fallback (`?cpu=1` URL param forces it). `main.cpp` owns the loop and flags.
- Heat is a per-cell `uint8_t` on non-GRID cells (docs/heat.md); balancing constants live in `src/game/heat_constants.hpp`.

## Assets & codegen

- Item icons/UI tiles are SVGs in `assets/` (hundreds of files) loaded **at runtime** from cwd (falls back to `../<path>`), so editing an SVG needs only a restart. The active item→icon mapping is `assets/items.csv` (`id,name,R,G,B,icon,svgPath,sway`) — must stay in sync with `src/game/config.hpp`.
- **WASM has no SkSVGDOM and no filesystem** (`-sFILESYSTEM=0`): the wasm Skia is built without expat, and `assets/` is absent in the browser. Item icons instead come from a build-time codegen step: CMake's wasm branch runs `python3 tools/gen_wasm_svg_assets.py`, which parses `assets/items.csv`, strips each referenced SVG down to compact per-shape path data (`d` verbatim + fill color + fill/stroke style) and writes `generated/wasm_svg_assets.h` into the build dir; `src/game/view/embedded_svg.cpp` rebuilds `PerPathSvg`s from it at startup via `SkParsePath` (present in libskia.wasm.a). Rendering is bit-identical to the native live-file parse. Chess pieces/glow and UI tiles (SkSVGDOM-based, `#if !defined(__EMSCRIPTEN__)` in `asset_manager`/`piece_library`/`ui_library`) remain native-only; web falls back to vector symbols for those.
- Native still loads SVGs live, so an SVG edit needs only a restart natively; on web it needs a rebuild (the custom command re-runs on any `assets/**/*.svg` change).
- Generated: `src/game/chess_assets.h` ← `python3 tools/gen_chess_assets.py` (bakes `assets/ChessWhite/element*.svg`, inlines CSS-class fills because SkSVGDOM ignores `<style>`). UI tiles ← `python3 tools/gen_ui_assets.py`.

## Misc

- `window_state.txt` is runtime-generated (window geometry persistence); gitignored.
- Commit messages use a `scope: summary` prefix style (e.g. `model: fix heat diffusion…`, `view: add …`).
