#pragma once

#include <stdarg.h>
#include <stddef.h>

#ifndef EOF
#define EOF (-1)
#endif

int vsprintf(char *__restrict str, const char *__restrict fmt, va_list arg);