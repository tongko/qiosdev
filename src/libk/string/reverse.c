#include <libk/string.h>

void reverse(char *str) {
	char *s = str;
	size_t	len = strlen(s), i, j;
	char	aux;

	for (i = len - 1, j = 0; i > j; i--, j++) {
		aux = s[i];
		s[i] = s[j];
		s[j] = aux;
	}
}