#pragma once
#include "gl.hpp" // IWYU pragma: keep

extern const char *vertSrc;
extern const char *fragSrc;

int compile(GLenum type, const char *src);
