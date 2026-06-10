#pragma once
#include "gl.hpp"

extern const char *vertSrc;
extern const char *fragSrc;

int compile(GLenum type, const char *src);
