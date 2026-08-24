cmake_minimum_required(VERSION 3.24)
project(fractory_blend2d CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Export a compile_commands.json so clangd (LSP) can resolve FetchContent
# includes like blend2d/blend2d.h. Symlink at repo root: compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" FRACTORY_VERSION)
string(STRIP "${FRACTORY_VERSION}" FRACTORY_VERSION)

# --- Blend2D (pinned to classic 0.0.18 API) ---
include(FetchContent)
FetchContent_Declare(
  blend2d
  GIT_REPOSITORY https://github.com/blend2d/blend2d
  GIT_TAG fc1061256cadc204ffa6a14c98923334df1bcfcc   # v0.0.18 (classic camelCase API)
)
option(BLEND2D_TEST "Build Blend2D tests" OFF)
option(BLEND2D_DEMOS "Build Blend2D demos" OFF)
option(BLEND2D_NO_NATVIS "Disable Blend2D natvis" ON)
option(BLEND2D_NO_JIT "Disable Blend2D JIT (self-contained build)" ON)
option(BLEND2D_STATIC "Build Blend2D as a static library" ON)
set(BLEND2D_NO_INSTALL ON CACHE BOOL "Disable Blend2D install" FORCE)

FetchContent_MakeAvailable(blend2d)

set(FRACTORY_SOURCES
    src/main.cpp src/font.cpp src/game/game.cpp src/game/game_view.cpp src/game/game_model.cpp
    src/game/interact_config.cpp third_party/svg2b2d/svg.cpp
    src/game/svg_assets.cpp
)

# Helper: attach Blend2D to a target (link + include the header root).
function(link_blend2d TARGET)
  target_link_libraries(${TARGET} PRIVATE blend2d)
  # Classic 0.0.18 layout: umbrella <blend2d.h> + subheaders live in src/.
  target_include_directories(${TARGET} PRIVATE "${blend2d_SOURCE_DIR}/src")
endfunction()

# Helper: attach vendored svg2b2d (SVG -> BLImage).
function(link_svg2b2d TARGET)
  target_include_directories(${TARGET} PRIVATE "${CMAKE_SOURCE_DIR}/third_party/svg2b2d")
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
  link_blend2d(fractory)
  link_svg2b2d(fractory)
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
link_blend2d(fractory)
link_svg2b2d(fractory)