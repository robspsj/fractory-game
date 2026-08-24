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
./build/fractory --empty-grid [N]          # start with an empty N×N grid
./build/fractory --vsync                   # cap FPS to display refresh (default: off = unlocked FPS)
./build/fractory --svg assets              # render item icons from SVG files in `assets/`
```
An on-screen HUD shows FPS plus a frame-time breakdown (Render / Copy /
Present) so you can see where time is actually spent.

With `--svg <dir>`, per-element icons are loaded from `element0.svg`..`element4.svg`
in that directory and rendered in place of the built-in vector symbols. If any
file is missing, that element falls back to its vector symbol. SVG rendering is
provided by the vendored [`svg2b2d`](https://github.com/Wiladams/svg2b2d) parser.
Each icon's SVG is re-rasterized at its exact on-screen pixel size (cached), so
icons stay **crisp while zooming** — the vector paths are re-rendered, not
bitmap-upscaled.

> This project builds Blend2D at **classic v0.0.18** (pin `fc1061256`), because
> svg2b2d targets the classic camelCase API (the current dev master renamed it to
> snake_case). Note: classic Blend2D's `blitImage` is a no-op when built with
> `BLEND2D_NO_JIT=ON`, so SVG icons are composited manually (src-over) onto the
> frame buffer rather than via `blitImage`.

## Build & run (web)

```
PATH="$PATH:/opt/homebrew/bin" emcmake cmake -S . -B build-wasm
PATH="$PATH:/opt/homebrew/bin" cmake --build build-wasm -j
python3 -m http.server 8000 --directory build-wasm
# open http://localhost:8000/
```

Blend2D is fetched and pinned via CMake `FetchContent` at a specific commit.