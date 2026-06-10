#pragma once
#include "../gl.hpp"
#include <vector>

void gameInit();
void gameUpdate(int mousePx, int mousePy, int winW, int winH);
void gameMouseDown(int mousePx, int mousePy, int winW, int winH);
void gameMouseUp(int mousePx, int mousePy, int winW, int winH);
void gameRender();
GLuint gameProgram();

// Testing API
void gameSetState(int grid[5][5]);
void gameGetState(int grid[5][5]);
