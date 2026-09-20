#include <libk/string.h>
#include <stddef.h>

char *strncpy(char *dest, const char *src, size_t n) {
	size_t i = strlen(src);

	// i = number of non-null bytes to copy
	memcpy(dest, src, i);

	// fill remaining space with '\0' to reach n bytes
	for (; i < n; i++) {
		dest[i] = '\0';
	}

	return dest;
}