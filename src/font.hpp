#pragma once
#include "gl.hpp" // IWYU pragma: keep

void initFont(GLuint mainProg);
void drawText(float x, float y, const char *text, float scale, float *vertsOut, int *outLen);
void drawTextGl(const float *verts, int len, GLuint mainProg, int winW, int winH);
