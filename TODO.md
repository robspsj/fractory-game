# TODO

## 🌟 Epic: Save State

- [ ] Recursive serialization — `getFullState`/`setFullState` must handle subgrid content, not just root grid
- [ ] `setFullState` must not orphan existing subgrid children — reuse or clean up orphaned nodes
- [ ] Decide on save format (binary vs text, file extension, location)
- [ ] Add save/load file I/O (SDL or platform file APIs)
- [ ] Hook save/load into controller — keyboard shortcuts, auto-save, load on startup
- [ ] Tests: round-trip save/load with nested subgrids

## 🔴 Bugs

- [ ] Hover highlight broken in subgrid focus mode
  - `renderCell()` calls `_model.rootChild(_hoverRow, _hoverCol)` but `rootChild()` always indexes into the root grid (index 0, size 5). When a subgrid is focused, `screenToGrid` returns coords relative to the subgrid (size 3), so hover highlights the wrong root-grid cell.
  - Files: `src/game/game_view.cpp:87-88`, `src/game/game_model.hpp:40-42`

- [ ] Shader has dead `aOffset` attribute
  - Vertex shader expects `aPos + aOffset` positioning, but view only uploads `aPos` and `aColor` (bakes all position data into `aPos`). `aOffset` defaults to (0,0). Shader was designed for instanced rendering but code uses flat batch.
  - Files: `src/shader.cpp:6`, `src/game/game_view.cpp:147-149`

- [ ] Save/restore (`setFullState`/`getFullState`) loses all subgrid content
  - `setFullState()` only restores root grid cells and allocates empty subgrids. Any subgrid content from serialized state is discarded. Tests work around this by never saving/restoring subgrid content.
  - File: `src/game/game_model.cpp:112-138`

- [ ] `setFullState()` orphans existing subgrid children when converting a cell to GRID
  - New children are `push_back()`ed onto `_nodes`, but any previously existing children for that cell remain as unreachable orphans — a memory leak in the flat array.
  - File: `src/game/game_model.cpp:120-128`

- [ ] Items with count 6–9 render as a single dot (visual bug)
  - `renderCellItems` defines layouts for counts 1–5; `default` falls through to 1 dot. Since item count = `rand() % 9 + 1` (range 1–9), 4/9 possible values display wrong.
  - File: `src/game/game_view.cpp:64`

## 🟡 Design / Correctness Issues

- [ ] Enable targeting subgrid cells with swap
  - `drop()` in the controller always uses `model.rootChild(row, col)` which maps to root grid cells only. To pick up from or drop onto cells inside a focused subgrid, the controller must resolve coordinates through the focus stack and compute the correct node index into the subgrid's children.
  - Requires: passing current focus state to `screenToGrid` / adding a `focusedNodeIndex()` helper on the view, then using that offset when computing drop targets.
  - Files: `src/game/game.cpp`, `src/game/game_view.cpp`, `src/game/game_model.cpp`

- [ ] Dropping onto a GRID cell swaps it destructively
  - `drop()` swap logic treats GRID same as an ITEM with non-matching ID: overwrites the GRID with the dragged ITEM and moves the GRID (with its `firstChild` pointer) to the drag source. A subgrid can be trivially destroyed and its children end up owned by a semantically wrong parent.
  - File: `src/game/game_model.cpp:82-89`

- [ ] `screenToGrid()` doesn't account for subgrid rendering position when focused
  - When focused, `screenToGrid` uses `pitch = _cellSize` and computes positions from root grid origin. But subgrids are rendered inside their parent cell at nested positions. Hit-testing within a focused subgrid produces wrong coordinates.
  - File: `src/game/game_view.cpp:158-185`

- [ ] Native build only supports macOS (not Linux)
  - `gl.hpp` includes `<OpenGL/gl.h>` unconditionally on non-Web, and CMakeLists only links OpenGL on `APPLE`. Linux has neither the header path nor the framework.
  - Files: `src/gl.hpp:5`, `CMakeLists.txt:36-38`

- [ ] Model state has no guard against concurrent access
  - Architecture is single-threaded today, but nothing prevents concurrent mutation if threading is added later.

## 🟢 Test Coverage Gaps

- [x] No tests verify GRID structural integrity after a swap
- [x] No tests for `cancelDrag()`
- [ ] No tests for focused subgrid interaction (enter/exit, hover, pickup/drop inside subgrid) — requires view/controller integration, can't test with model-only CSV runner

## 🟣 Organization / Consistency

- [x] `test_runner.hpp` is header-only while everything else uses .hpp/.cpp — inconsistent pattern
- [x] `game.hpp` exposes C-style free functions wrapping a C++ class — obscures the object-oriented design
- [ ] `serve.sh` is duplicated at repo root and in `tools/`
