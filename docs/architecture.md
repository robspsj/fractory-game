# fractory — Architecture Overview

A small C++17 game that matches a **single codebase** to two targets: **native macOS** and **WebAssembly (browser)** via Emscripten.

## Build System

| File | Role |
|---|---|
| `CMakeLists.txt` | Dual-target CMake config. `EMSCRIPTEN` flag switches between native SDL3/macOS OpenGL and Emscripten SDL3/GLES2 |
| `build.sh` | Wrapper script: `./build.sh` → native, `./build.sh web` → Emscripten → `.wasm` + `.html` |
| `index.html` | Emscripten shell HTML; full-screen canvas with no scrolling |

## Source Layout

```
src/
├── main.cpp          ← entry point, render loop, event pump
├── gl.hpp            ← OpenGL header shim (macOS vs GLES2)
├── shader.hpp/cpp    ← GLSL source strings + compile() helper
├── font.hpp/cpp      ← 8×8 bitmap font renderer (15 glyphs)
└── game/
    ├── game.hpp      ← game API: init / update / mouse / render
    └── game.cpp      ← 5×5 grid game state + rendering
```

## Component Responsibilities

### `main.cpp` — Application Shell
- Initializes SDL3, creates an OpenGL window (800×600, resizable)
- Runs the **render loop**: poll events → clear → `gameRender()` → FPS overlay → swap
- On Emscripten: uses `emscripten_set_main_loop(frame, 0, 1)` instead of a while-loop
- Handles resize events and forwards mouse events into the game layer
- FPS counter rendered via the font system

### `gl.hpp` — Platform Abstraction
- One-liner: includes `SDL_opengles2.h` on Emscripten, `OpenGL/gl.h` on macOS. Every other file just `#include "gl.hpp"` and gets the right GL API.

### `shader.hpp/cpp` — Shader Compilation
- Holds the vertex/fragment shader source as C string literals (different frag shaders for GLES2 vs desktop)
- `compile(type, src)` → creates, sources, compiles, and error-checks a shader object
- Game shader: simple position + color pass (`aPos` + `aColor` + `uPos` uniform)

### `font.hpp/cpp` — Bitmap Font
- 15-character 8×8 pixel font (space, 0–9, `:`, `F`, `P`, `S`)
- Builds a texture atlas from raw bitmaps, then renders text as textured quads
- Separate shader program with screen-space positioning + texture sampling
- Used only for the FPS overlay (green text, top-right corner)

### `game/game.cpp` — Game Logic
The core game: a **5×5 grid** of **5 colored element types**. Currently implements:
- **Hover highlighting** — brightens the cell under the cursor
- **Click-and-drag swapping** — swap two cells by dragging from one to another; drag ghost follows cursor
- Screen-space → grid coordinate conversion via `cellAt()`
- Renders all quads as a single `glDrawArrays` batch via a VBO (stream draw)

## Data Flow

```
Event (SDL3)
  ↓
main.cpp → gameUpdate() / gameMouseDown() / gameMouseUp()
  ↓
game state (grid[5][5], hover, drag)
  ↓
gameRender() → generate vertex batch → VBO → glDrawArrays
  ↓
drawFpsOverlay() → font verts → text VBO → glDrawArrays
  ↓
SDL_GL_SwapWindow
```

## Key Design Pattern
The **conditional compilation** approach (`#ifdef __EMSCRIPTEN__` in `main.cpp`, `#ifdef EMSCRIPTEN` in CMake, `#ifdef __EMSCRIPTEN__` in `gl.hpp` and `shader.cpp`) allows the same C++ source to compile to both platforms with minimal platform-specific branching — the only real divergence is the main loop structure and which GL header to use.
