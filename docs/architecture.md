# fractory — Architecture Overview

A small C++17 game that matches a **single codebase** to two targets: **native macOS** and **WebAssembly (browser)** via Emscripten.

## Build System

| File | Role |
|---|---|
| `CMakeLists.txt` | Dual-target CMake config. `EMSCRIPTEN` flag switches between native SDL3/macOS OpenGL and Emscripten SDL3/GLES2 |
| `index.html` | Emscripten shell HTML; full-screen canvas with no scrolling |

## Source Layout

```
src/
├── main.cpp                 ← entry point, render loop, event pump
├── gl.hpp                   ← OpenGL header shim (macOS vs GLES2)
├── shader.hpp/cpp           ← GLSL source strings + compile() helper
├── font.hpp/cpp             ← 8×8 bitmap font renderer (15 glyphs)
├── print_state.hpp/cpp      ← console debug-state printer (ANSI)
├── test_runner.hpp          ← CSV-driven model test harness (header-only)
└── game/
    ├── game.hpp/cpp         ← Controller: input handling, mouse state machine
    ├── game_model.hpp/cpp   ← Model: game state, logic, pick-up/drop/swap
    └── game_view.hpp/cpp    ← View: OpenGL rendering, camera, hit-testing
```

## MVC Architecture

The game layer follows a **Model-View-Controller** pattern:

```
game.cpp (Controller)
  ├─ owns: unique_ptr<GameModel>
  ├─ owns: unique_ptr<GameView>
  └─ mediates: input → Model mutations → View render

game_model.cpp (Model)
  └─ pure game state + logic: cells, drag, pickUp/drop/swap

game_view.cpp (View)
  └─ rendering + camera: pan, zoom, subgrid focus, coordinate transforms
```

### Model (`game_model.cpp`)

**Data structures:**
- `Cell` — discriminated union with `CellType { EMPTY, ITEM, GRID }`
  - `ItemData { int id, count }` — 5 element types (0–4), stackable
  - `GridData { int firstChild, int size }` — index into flat node array + dimension
- `_nodes` — `std::vector<Cell>` flat array; root grid is always index 0

**Key methods:**
- `init(seed)` — populates a random 5×5 grid (50% EMPTY, 40% ITEM, 10% nested 3×3 GRID)
- `pickUp(nodeIndex, amount)` — removes items from cell, stores drag state
- `drop(nodeIndex)` — place dragged items; merges stacks of same ID, **swaps** on type/id mismatch, restores on invalid drop
- `cancelDrag()` — returns items to source cell
- `getFullState()` / `setFullState()` — serialize/deserialize grid for save/load/test

Cells encode as integers: `-2` = GRID, `-1` = EMPTY, `>= 0` = ITEM id (count in paired slot).

### View (`game_view.cpp`)

**Camera system:**
- `_zoom` (0.1–10.0), `_panX/_panY` — world-to-screen transform
- `screenToWorld()` / `screenToGrid()` — invert projection for hit-testing
- `startPan()` / `continuePan()` / `endPan()` — mouse drag-to-pan

**Focus stack:**
- `_focusStack` — `std::stack<int>` of node indices; drill into nested subgrids via right-click
- `focusGrid(nodeIndex)` — push (enter subgrid); `unfocusGrid()` — pop (exit)
- Only GRID-type cells accept focus; clicking outside while focused pops back

**Rendering pipeline:**
```
render(winW, winH)
  ├─ get root node from focus stack (or index 0 if empty)
  ├─ glUseProgram(_prog)
  │   glUniform2f(uPos, panX, panY)
  │   glUniform1f(uZoom, zoom)
  │   glUniform1f(uAspect, aspect)
  ├─ renderGrid(v, rootIdx, cx, cy, totalSize, depth=0)
  │   └─ for each cell:
  │       ├─ addQuad(v, ...) → 6 verts (x,y + r,g,b)
  │       ├─ if ITEM: renderCellItems() → 1–5 colored dots
  │       └─ if GRID: recurse (depth<3) or red indicator dot
  ├─ upload all verts via glBufferData (STREAM_DRAW)
  ├─ glDrawArrays(GL_TRIANGLES)
  └─ if dragging: render item ghost at cursor world position
```

All geometry is baked into one flat array (`verts[1024*1024]`) and drawn in a single draw call. Recursive subgrid rendering is capped at depth 3.

### Controller (`game.cpp`)

**Mouse state machine:**
```
NONE → DOWN_PENDING (on any button press)
         ├─ +250ms + moved >5px → PANNING (middle/right) or DRAGGING_ITEM (left)
         ├─ quick release → perform action (focus/unfocus for right,
         │   pickUp/drop for left)
         └─ release on empty space while dragging → cancelDrag()

DRAGGING_ITEM → drop() at target or cancelDrag() on release
PANNING → endPan() on release
HOVERING_GRID_FOR_FOCUS → set on motion over GRID cell with no focused grid
```

**Input mapping:**

| Event | Action |
|---|---|
| Left button | Pick up / drop items; drag-to-swap |
| Middle button | Pan view |
| Right button | Focus/unfocus subgrids; pan view |
| Mouse wheel | Zoom in/out (×1.1) |

## Data Flow

```
SDL3 Event
  ↓
main.cpp → gameUpdate / gameMouseDown / gameMouseUp / gameMouseWheel
  ↓
game.cpp (Controller)
  ├─ game_model.cpp (model mutation: pickUp/drop/cancel)
  └─ game_view.cpp (camera: pan/zoom/focus)
  ↓
gameRender(winW, winH)
  ├─ view.render() → single VBO batch → glDrawArrays
  └─ drawFpsOverlay() → font text VBO → glDrawArrays (separate shader)
  ↓
SDL_GL_SwapWindow
```

## Application Shell (`main.cpp`)

- Boots SDL3 + OpenGL 2.1 context (800×600, resizable)
- `--test <file.csv>` argument runs the CSV test harness and exits
- Render loop: poll events → forward to game controller → clear → `gameRender()` → FPS overlay → swap
- Emscripten path: `emscripten_set_main_loop(frame, 0, 1)` instead of a while-loop
- FPS counter: green text via font system, updates every 200ms

## Support Modules

### `print_state.cpp` — Console Debug
ANSI-escaped terminal output of full grid state (cell contents, drag state, recursive subgrids). Called on every pick-up/drop/cancel for development visibility.

### `test_runner.hpp` — CSV Test Harness
Header-only `ModelTestRunner`. Parses CSV files with `init`, `pickup`, `drop`, `expect_cell`, `expect_drag` commands and runs them against `GameModel` directly (no rendering, no GL). Invoked via `./fractory --test <file.csv>`.

### `font.cpp` — Bitmap Font
15-character 8×8 pixel font atlas (space, `0`–`9`, `:`, `F`, `P`, `S`). Separate shader program, screen-space coordinates, alpha blending. Used only for FPS overlay.

### `shader.cpp` — Shader Resources
GLSL vertex/fragment source strings. Vertex shader takes `aPos` (quad shape), `aOffset` (position), `aColor` with `uPos`/`uZoom`/`uAspect` uniforms.

## Platform Abstraction

Conditional compilation (`#ifdef __EMSCRIPTEN__`) handles:
- GL headers (`gl.hpp`): `SDL_opengles2.h` on web, `OpenGL/gl.h` on macOS
- Shader precision qualifiers (`precision mediump float`)
- Main loop structure (callback vs while-loop)
- Window resize handling
- No cleanup on web (browser-managed)

## Key Design Patterns

- **MVC split** — model has zero rendering/input dependencies; view is pure GL; controller orchestrates
- **Single-pass batching** — all grid geometry in one vertex array, one draw call
- **Flat node array** — recursive grid hierarchy stored in `std::vector<Cell>` with index-based parent-to-child links
- **Union cell storage** — memory-efficient discriminated union for EMPTY/ITEM/GRID
- **CSV-driven model tests** — no GL context needed for unit-testing game logic
