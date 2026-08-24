# fractory (Blend2D port)

A port of [fractory_cpp](https://github.com/blend2d/blend2d) rendering from OpenGL
to **Blend2D**, with an SDL2 front-end. Runs natively (system window) and on the
web (WASM canvas), from one shared codebase.

The game logic (`GameModel`) is inherited verbatim from fractory_cpp. The renderer
is rewritten: cells and item dots are drawn with Blend2D `BLContext` into a `BLImage`,
then presented as a packed RGBA buffer via an SDL2 texture (native) or `putImageData`
on a 2D canvas (web).

## Requirements

- macOS (or Linux) with a C++17 compiler, CMake >= 3.24
- SDL2 (`brew install sdl2`)
- For the web build: Emscripten (`brew install emscripten`)

## Build & run (native)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/fractory
```

Controls: left-click = pick up / drop item, shift+click = interact,
middle-drag = pan, scroll = zoom, F1 = reset view.

Launch flags (all optional):
```
./build/fractory --empty-grid [N]           # start with an empty N×N grid
./build/fractory --grid-limit 2000000       # cap on how many GRID nodes the fractal can grow (default 2,000,000)
./build/fractory --max-depth 100            # max recursion depth to render/resolve (default 100)
./build/fractory --min-clip 0.0005          # cull cells smaller than this (NDC); lower = draw more tiny cells
```
Raising `--grid-limit` lets the world grow deeper/larger; lowering `--min-clip`
and/or raising `--max-depth` renders more nested sub-grid cells when zoomed in.

## Build & run (web)

```
PATH="$PATH:/opt/homebrew/bin" emcmake cmake -S . -B build-wasm
PATH="$PATH:/opt/homebrew/bin" cmake --build build-wasm -j
python3 -m http.server 8000 --directory build-wasm
# open http://localhost:8000/
```

Blend2D is fetched and pinned via CMake `FetchContent` at a specific commit.