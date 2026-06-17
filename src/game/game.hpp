#pragma once
#include "../gl.hpp"

void gameInit(unsigned int seed = 0);
void gameUpdate(int mousePx, int mousePy, int winW, int winH);
void gameMouseDown(int button, int mousePx, int mousePy, int winW, int winH);
void gameMouseUp(int button, int mousePx, int mousePy, int winW, int winH);
void gameMouseWheel(float dx, float dy);
void gameRender(int winW, int winH);
GLuint gameProgram();

void gameSetFullState(int* inData);
void gameGetFullState(int* outData);
void gameGetDragState(int& outId, int& outCount);
