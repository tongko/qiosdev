#include <libk/stdlib.h>
#include <libk/string.h>
#include <stdint.h>

size_t itoa(int value, char *str, int base, const char *digit_str) {

	//	Validate base
	if (base != 2 && base != 8 && base != 10 && base != 16) {
		*str = '\0';
		return 0;
	}

	char *p = str;
	int quotient = value;
	size_t sz = 0;

	//	Conversion. Number is reversed.
	do {
		const int tmp = quotient / base;
		*p++ = digit_str[abs(quotient - (tmp * base))];
		quotient = tmp;
		sz++;
	} while (quotient);

	if (value < 0) {
		*p++ = '-';
		sz++;
	}

	*p = '\0';
	reverse(str);

	return sz;
}

size_t ltoa(long long value, char *str, int base, const char *digit_str) {

	//	Validate base
	if (base != 2 && base != 8 && base != 10 && base != 16) {
		*str = '\0';
		return 0;
	}

	char *p = str;
	long long quotient = value;
	size_t sz = 0;

	//	Conversion. Number is reversed.
	do {
		const long long tmp = quotient / base;
		*p++ = digit_str[quotient - (tmp * base)];
		quotient = tmp;
		sz++;
	} while (quotient);

	if (value < 0) {
		*p++ = '-';
		sz++;
	}

	*p = '\0';
	reverse(str);

	return sz;
}

size_t utoa(unsigned int value, char *str, int base, const char *digit_str) {

	//	Validate base
	if (base != 2 && base != 8 && base != 10 && base != 16) {
		*str = '\0';
		return 0;
	}

	char *p = str;
	unsigned int quotient = value;
	size_t sz = 0;

	//	Conversion. Number is reversed.
	do {
		const unsigned int tmp = quotient / base;
		*p++ = digit_str[quotient - (tmp * base)];
		quotient = tmp;
		sz++;
	} while (quotient);

	*p = '\0';
	reverse(str);

	return sz;
}

size_t ultoa(unsigned long long value, char *str, int base, const char *digit_str) {

	//	Validate base
	if (base != 2 && base != 8 && base != 10 && base != 16) {
		*str = '\0';
		return 0;
	}

	char *p = str;
	unsigned long long quotient = value;
	size_t sz = 0;

	//	Conversion. Number is reversed.
	do {
		const unsigned long long tmp = quotient / base;
		*p++ = digit_str[quotient - (tmp * base)];
		quotient = tmp;
		sz++;
	} while (quotient);

	*p = '\0';
	reverse(str);

	return sz;
}
