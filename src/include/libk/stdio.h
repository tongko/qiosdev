#pragma once

#include <stdarg.h>
#include <stddef.h>

#ifndef EOF
#define EOF (-1)
#endif

int snprintf(char *__restrict str, size_t sz, const char *__restrict fmt, ...);

int vsprintf(char *__restrict str, const char *__restrict fmt, va_list arg);

int vsnprintf(char *__restrict str, size_t sz, const char *__restrict fmt, va_list arg);