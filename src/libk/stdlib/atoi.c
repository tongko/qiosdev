#include <libk/stdlib.h>

unsigned int atoi(const char *str) {
	if (!str) {
		return 0;
	}

	unsigned int result = 0;

	for (size_t i = 0; str[i] != '\0'; i++) {
		if (!ISDIGIT(str[i])) {
			return result;
		}
		result = result * 10 + str[i] - 48; // 48 == '0'
	}

	return result;
}
