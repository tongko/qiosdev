#pragma once

#include <stddef.h>
#include <stdint.h>

__attribute__((__noreturn__)) void abort(void);

int abs(int x);
long absl(long x);

#ifndef ISDIGIT
#define ISDIGIT(x) (x >= 0x30 && x <= 0x39)
#endif //	ISDIGIT

size_t itoa(int value, char *str, int base, const char *digit_str);
size_t utoa(unsigned int value, char *str, int base, const char *digit_str);
size_t ltoa(long long value, char *str, int base, const char *digit_str);
size_t ultoa(unsigned long long value, char *str, int base, const char *digit_str);
unsigned int atoi(const char *str);

#define MAX(a, b)                                                                                                      \
	({                                                                                                                  \
		__typeof__(a) _a = (__typeof__(a))(a);                                                                           \
		__typeof__(a) _b = (__typeof__(a))(b);                                                                           \
		_a > _b ? _a : _b;                                                                                               \
	})

#define MIN(a, b)                                                                                                      \
	({                                                                                                                  \
		__typeof__(a) _a = (__typeof__(a))(a);                                                                           \
		__typeof__(a) _b = (__typeof__(a))(b);                                                                           \
		_a < _b ? _a : _b;                                                                                               \
	})

unsigned int min(unsigned int a, unsigned int b);
unsigned int max(unsigned int a, unsigned int b);