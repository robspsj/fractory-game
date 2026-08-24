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

## Build & run (web)

```
PATH="$PATH:/opt/homebrew/bin" emcmake cmake -S . -B build-wasm
PATH="$PATH:/opt/homebrew/bin" cmake --build build-wasm -j
python3 -m http.server 8000 --directory build-wasm
# open http://localhost:8000/
```

Blend2D is fetched and pinned via CMake `FetchContent` at a specific commit.