#include <libk/string.h>
#include <stddef.h>

void *memcpy(void *__restrict dest, const void *__restrict src, size_t n) {
	size_t i = 0;
	size_t t = 0;

	void *pdst = dest;
	if (n % 4 == 0) {
		unsigned int *id = (unsigned int *)pdst;
		const unsigned int *is = (const unsigned int *)src;
		t = n / 4;
		while (i++ < t) {
			*(id++) = *(is++);
		}
	} else if (n % 2 == 0) {
		unsigned short *sd = (unsigned short *)pdst;
		const unsigned short *ss = (const unsigned short *)src;
		t = n / 2;
		while (i++ < t) {
			*(sd++) = *(ss++);
		}
	} else {
		unsigned char *d = (unsigned char *)pdst;
		const unsigned char *s = (const unsigned char *)src;
		while (i++ < n) {
			*(d++) = *(s++);
		}
	}

	return dest;
}