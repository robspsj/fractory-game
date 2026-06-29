cmake_minimum_required(VERSION 3.16)
project(fractory CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FRACTORY_DEBUG "Enable debug commands" OFF)
if(FRACTORY_DEBUG)
    add_compile_definitions(FRACTORY_DEBUG)
endif()

# --- Web (Emscripten) ---
if(EMSCRIPTEN)
    add_executable(fractory src/main.cpp src/shader.cpp src/font.cpp src/print_state.cpp src/test_runner.cpp src/game/game.cpp src/game/game_model.cpp src/game/game_view.cpp)
    set_target_properties(fractory PROPERTIES OUTPUT_NAME "index")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=3 -Os -Wno-experimental")
    target_link_options(fractory PRIVATE
        -sUSE_SDL=3
        -sFULL_ES2=1
        -sGL_ENABLE_GET_PROC_ADDRESS=1
        --shell-file ${CMAKE_SOURCE_DIR}/index.html
        -sALLOW_MEMORY_GROWTH=1
        -sFILESYSTEM=0
        -sINITIAL_MEMORY=8MB
    )
    return()
endif()

# --- Native (macOS/Linux) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

add_executable(fractory src/main.cpp src/shader.cpp src/font.cpp src/print_state.cpp src/test_runner.cpp src/game/game.cpp src/game/game_model.cpp src/game/game_view.cpp)
target_include_directories(fractory PRIVATE ${SDL3_INCLUDE_DIRS})
target_link_directories(fractory PRIVATE ${SDL3_LIBRARY_DIRS})
target_link_libraries(fractory PRIVATE ${SDL3_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_options(fractory PRIVATE ${SDL3_CFLAGS_OTHER})
target_compile_definitions(fractory PRIVATE GL_SILENCE_DEPRECATION)

if(APPLE)
    find_library(OPENGL_LIB OpenGL)
    target_link_libraries(fractory PRIVATE ${OPENGL_LIB})
endif()
