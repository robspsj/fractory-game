# fractory (Blend2D port)

A port of [fractory_cpp](https://github.com/blend2d/blend2d) rendering from OpenGL
to **Blend2D**, with an SDL2 front-end. Runs natively (system window) and on the
web (WASM canvas), from one shared codebase.

The game logic (`GameModel`) is inherited verbatim from fractory_cpp. The renderer
is rewritten: cells and item dots are drawn with Blend2D `BLContext` into a `BLImage`,
then presented on screen. Natively this rasterizes **directly into the SDL window
surface's pixel buffer** (a `BLImage::createFromData` wrap of the surface memory) —
no GPU texture, no fullscreen quad; on the web it presents via `putImageData` on a
2D canvas.

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

Controls: left-click = pick up / drop item, right-click = act (in the gap
between cells = reaction; on a cell = craft when a wand is held, else load a
machine / spawn), right-hold = warm the hovered cell (quantum heat pump),
middle-drag = pan, scroll = zoom, F1 = reset view, F2 = recipe book.

Launch flags (all optional):
```
./build/fractory --empty-grid [N]          # start with an empty N×N grid
./build/fractory --vsync                   # cap FPS to display refresh (default: off = unlocked FPS)
./build/fractory --svg <dir>               # render item icons from SVG files in <dir> (default: assets/)
./build/fractory --no-svg                  # disable SVG icons (use built-in vector symbols)
```
An on-screen HUD shows FPS plus a frame-time breakdown (Render / Copy /
Present) so you can see where time is actually spent.

SVG icons are enabled by default (loaded from `assets/`): per-element icons are
loaded from `element0.svg`..`element4.svg` in that directory and rendered in
place of the built-in vector symbols. If any file is missing, that element falls
back to its vector symbol. Use `--svg <dir>` for a custom directory or
`--no-svg` to disable. SVG rendering is provided by the vendored
[`svg2b2d`](https://github.com/Wiladams/svg2b2d) parser.
Each icon's SVG is re-rasterized at its exact on-screen pixel size (cached), so
icons stay **crisp while zooming** — the vector paths are re-rendered, not
bitmap-upscaled.

> This project builds Blend2D at **classic v0.0.18** (pin `fc1061256`), because
> svg2b2d targets the classic camelCase API (the current dev master renamed it to
> snake_case). Note: classic Blend2D's `blitImage` is a no-op when built with
> `BLEND2D_NO_JIT=ON`, so SVG icons are composited manually (src-over) onto the
> frame buffer rather than via `blitImage`. Likewise the non-JIT rasterizer cannot
> produce radial gradients (only solid + linear), so SVG radial-gradient fills are
> software-rasterized in `svg_assets` (sentinel-marked + per-pixel fill), giving
> correct gradients on both native and WASM.

## Build & run (web)

```
PATH="$PATH:/opt/homebrew/bin" emcmake cmake -S . -B build-wasm
PATH="$PATH:/opt/homebrew/bin" cmake --build build-wasm -j
python3 -m http.server 8000 --directory build-wasm
# open http://localhost:8000/
```

Blend2D is fetched and pinned via CMake `FetchContent` at a specific commit.