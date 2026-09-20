#include <libk/string.h>

#include <stddef.h>

size_t strlen(const char *str) {
	size_t i;
	char *s = (char *)str;
	for (i = 0; *s++; i++) {
	}

	return i;
}