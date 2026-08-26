cmake_minimum_required(VERSION 3.24)
project(fractory_skia CXX)

set(CMAKE_CXX_STANDARD 17)
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
    src/game/game.cpp
    src/game/game_view.cpp
    src/game/game_model.cpp
    src/game/interact_config.cpp
)

# Helper: attach Skia to a target (include the header root so
# `#include "include/core/SkCanvas.h"` resolves, and link the archive).
# LIBDIR is "out/native" or "out/wasm"; Skia names wasm archives *.wasm.a.
function(link_skia TARGET LIBDIR)
  target_include_directories(${TARGET} PRIVATE "${SKIA_DIR}")
  if(EMSCRIPTEN)
    target_link_libraries(${TARGET} PRIVATE
        "${SKIA_DIR}/${LIBDIR}/libskia.wasm.a"
        "${SKIA_DIR}/${LIBDIR}/libskcms.wasm.a")
  else()
    target_link_libraries(${TARGET} PRIVATE "${SKIA_DIR}/${LIBDIR}/libskia.a")
  endif()
endfunction()

# --- Web (Emscripten) ---
if(EMSCRIPTEN)
  add_executable(fractory ${FRACTORY_SOURCES})
  target_compile_definitions(fractory PRIVATE FRACTORY_VERSION="${FRACTORY_VERSION}")
  set_target_properties(fractory PROPERTIES OUTPUT_NAME "index")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  target_compile_options(fractory PRIVATE -O2 -Wno-experimental)
  target_link_options(fractory PRIVATE
      -sUSE_SDL=2
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

# --- Native (macOS/Linux), SDL2 ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(fractory ${FRACTORY_SOURCES})
target_include_directories(fractory PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_directories(fractory PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(fractory PRIVATE ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_options(fractory PRIVATE ${SDL2_CFLAGS_OTHER})
target_compile_definitions(fractory PRIVATE FRACTORY_VERSION="${FRACTORY_VERSION}")
link_skia(fractory "out/native")
# Skia CPU raster needs pthread + dl + math even though SDL is pulled first.
find_package(Threads REQUIRED)
target_link_libraries(fractory PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)