cmake_minimum_required(VERSION 3.24)
project(fractory_skia CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Export a compile_commands.json so clangd (LSP) can resolve Skia includes.
# Symlink at repo root: compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ninja gives fast, incremental rebuilds (our goal: quick iterating). Ninja is
# installed via Homebrew; `cmake -G Ninja` also works explicitly.
if(NOT DEFINED CMAKE_GENERATOR)
  set(CMAKE_GENERATOR "Ninja")
endif()
if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

file(READ "${CMAKE_SOURCE_DIR}/VERSION" FRACTORY_VERSION)
string(STRIP "${FRACTORY_VERSION}" FRACTORY_VERSION)

# --- Skia (prebuilt once via its own GN/ninja build; we just link it) ---
# Native:   <skia>/out/native/libskia.a   (CPU raster, arm64)
# Web:      <skia>/out/wasm/libskia.a     (Emscripten, CPU raster)
set(SKIA_DIR "${CMAKE_SOURCE_DIR}/../skia")

set(FRACTORY_SOURCES
    src/main.cpp
    src/font.cpp
    src/test_runner.cpp
    src/print_state.cpp
    src/game/game.cpp
    src/game/view/game_view.cpp
    src/game/view/layout_engine.cpp
    src/game/view/coordinate_mapper.cpp
    src/game/view/picking_engine.cpp
    src/game/view/camera_controller.cpp
    src/game/view/anchor_manager.cpp
    src/game/view/icon_renderer.cpp
    src/game/view/cell_renderer.cpp
    src/game/view/badge_renderer.cpp
    src/game/view/background_renderer.cpp
    src/game/view/asset_manager.cpp
    src/game/view/embedded_svg.cpp
    src/game/view/piece_library.cpp
    src/game/view/ui_library.cpp
    src/game/view/per_path_svg.cpp
    src/game/model/game_model.cpp
    src/game/model/empty_cell.cpp
    src/game/model/item_cell.cpp
    src/game/model/grid_cell.cpp
    src/game/model/station_cell.cpp
    src/game/model/reserved_cell.cpp
    src/game/particle.cpp
    src/game/interact_config.cpp
)

# Helper: attach Skia to a target (include the header root so
# `#include "include/core/SkCanvas.h"` and `"modules/svg/include/SkSVGDOM.h"`
# resolve, and link the archives). LIBDIR is "out/native" or "out/wasm"; Skia
# names wasm archives *.wasm.a.
# Native links modules/svg (libsvg, which provides SkSVGDOM) + its shapier/resource
# deps + system expat; its `skia_use_expat` is on (system expat). WASM has no
# system expat, so it stays SVG-less (web falls back to vector symbols).
function(link_skia TARGET LIBDIR)
  target_include_directories(${TARGET} PRIVATE "${SKIA_DIR}")
  if(EMSCRIPTEN)
    target_link_libraries(${TARGET} PRIVATE
        "${SKIA_DIR}/${LIBDIR}/libskia.wasm.a"
        "${SKIA_DIR}/${LIBDIR}/libskcms.wasm.a")
  else()
    target_link_libraries(${TARGET} PRIVATE
        "${SKIA_DIR}/${LIBDIR}/libskia.a"
        "${SKIA_DIR}/${LIBDIR}/libsvg.a"
        "${SKIA_DIR}/${LIBDIR}/libskshaper.a"
        "${SKIA_DIR}/${LIBDIR}/libskresources.a")
  endif()
endfunction()

# --- Web (Emscripten) ---
if(EMSCRIPTEN)
  # No SkSVGDOM (expat-less) and no filesystem on web: bake the item icons
  # referenced by assets/items.csv into a compact generated header. The
  # generator normalizes every shape to absolute M/L/C/Q path data at build
  # time, so the wasm runtime only needs SkParsePath (present in libskia.wasm.a).
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  file(GLOB_RECURSE FRACTORY_ICON_SVGS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/*.svg")
  set(FRACTORY_SVG_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/wasm_svg_assets.h")
  add_custom_command(OUTPUT "${FRACTORY_SVG_HEADER}"
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/gen_wasm_svg_assets.py"
              "${CMAKE_SOURCE_DIR}" "${FRACTORY_SVG_HEADER}"
      DEPENDS "${CMAKE_SOURCE_DIR}/assets/items.csv"
              "${CMAKE_SOURCE_DIR}/tools/gen_wasm_svg_assets.py"
              ${FRACTORY_ICON_SVGS}
      COMMENT "Generating embedded SVG icon data (wasm)")
  add_custom_target(gen_wasm_svg_assets DEPENDS "${FRACTORY_SVG_HEADER}")
  # Same story for the model's interaction data (station recipes): no
  # filesystem on web, so the CSV is embedded verbatim as a string literal and
  # parsed by the same loader the native build uses.
  set(FRACTORY_INTERACT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/wasm_interact_data.h")
  add_custom_command(OUTPUT "${FRACTORY_INTERACT_HEADER}"
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/gen_wasm_interact_data.py"
              "${CMAKE_SOURCE_DIR}" "${FRACTORY_INTERACT_HEADER}"
      DEPENDS "${CMAKE_SOURCE_DIR}/assets/station_recipes.csv"
              "${CMAKE_SOURCE_DIR}/tools/gen_wasm_interact_data.py"
      COMMENT "Generating embedded interaction data (wasm)")
  add_custom_target(gen_wasm_interact_data DEPENDS "${FRACTORY_INTERACT_HEADER}")
  add_executable(fractory ${FRACTORY_SOURCES} src/webgl_gpu.cpp)
  add_dependencies(fractory gen_wasm_svg_assets gen_wasm_interact_data)
  target_include_directories(fractory PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(fractory PRIVATE FRACTORY_VERSION="${FRACTORY_VERSION}")
  set_target_properties(fractory PROPERTIES OUTPUT_NAME "index")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  target_compile_options(fractory PRIVATE -O2 -Wno-experimental)
  target_link_options(fractory PRIVATE
      -sUSE_SDL=3
      -sMIN_WEBGL_VERSION=1
      -sMAX_WEBGL_VERSION=2
      --shell-file ${CMAKE_SOURCE_DIR}/web/index.html
      -sALLOW_MEMORY_GROWTH=1
      -sFILESYSTEM=0
      -sINITIAL_MEMORY=16MB
      -sNO_EXIT_RUNTIME=1
      -sEXPORTED_RUNTIME_METHODS=HEAPU8
  )
  link_skia(fractory "out/wasm")
  return()
endif()

# --- Native (macOS/Linux), SDL3 ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

# Metal GPU present path: the ObjC++ glue (metal_glue.mm) creates the MTLDevice/
# command queue + CAMetalLayer and presents the drawable. ARC is required for the
# CFBridging calls in that file.
enable_language(OBJCXX)
add_executable(fractory ${FRACTORY_SOURCES} src/metal_gpu.cpp src/metal_glue.mm)
set_source_files_properties(src/metal_glue.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_include_directories(fractory PRIVATE ${SDL3_INCLUDE_DIRS})
target_link_directories(fractory PRIVATE ${SDL3_LIBRARY_DIRS})
target_link_libraries(fractory PRIVATE ${SDL3_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_options(fractory PRIVATE ${SDL3_CFLAGS_OTHER})
target_compile_definitions(fractory PRIVATE FRACTORY_VERSION="${FRACTORY_VERSION}")
link_skia(fractory "out/native")
# Skia CPU raster needs pthread + dl + math even though SDL is pulled first.
find_package(Threads REQUIRED)
# expat: Skia's SkXMLParser/SkSVGDOM link against the system expat
# (skia_use_system_expat defaults on for official builds).
target_link_libraries(fractory PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m expat)
# Metal Ganesh backend + the ObjC++ glue link these Apple frameworks.
if(APPLE)
  target_link_libraries(fractory PRIVATE
      "-framework Metal" "-framework MetalKit" "-framework QuartzCore"
      "-framework CoreGraphics" "-framework Foundation" "-framework IOSurface"
      "-framework CoreFoundation" "-framework Cocoa")
endif()