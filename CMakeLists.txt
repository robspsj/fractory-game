cmake_minimum_required(VERSION 3.16)
project(fractory CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Web (Emscripten) ---
if(EMSCRIPTEN)
    add_executable(fractory src/main.cpp)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2")
    target_link_options(fractory PRIVATE
        -sUSE_SDL=2
        -sFULL_ES2=1
        -sGL_ENABLE_GET_PROC_ADDRESS=1
        --shell-file ${CMAKE_SOURCE_DIR}/index.html
        -sALLOW_MEMORY_GROWTH=1
        -o fractory.html
    )
    return()
endif()

# --- Native (macOS/Linux) ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(fractory src/main.cpp)
target_include_directories(fractory PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(fractory PRIVATE ${SDL2_LIBRARIES} ${OPENGL_gl_LIBRARY})
