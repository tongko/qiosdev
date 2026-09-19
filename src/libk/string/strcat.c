#include <libk/string.h>

char *strcat(char *dest, const char *src) {
	char *	  p = dest;
	const char *s = src;
	while (*p++) {}
	p--;

	while (*s != '\0') {
		*(p++) = *(s++);
	}

	*p = '\0';
	return dest;
}
