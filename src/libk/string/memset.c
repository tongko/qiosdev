#include <libk/string.h>
#include <stddef.h>

void *memset(void *str, int c, size_t n) {
	unsigned char *s = (unsigned char *)str;
	unsigned char ch = (unsigned char)c;

	while (n--) {
		*s++ = ch;
	}

	return str;
}
