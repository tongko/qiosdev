#include <libk/stdlib.h>

int abs(int x) {
	int y = x >> 31;
	return (x ^ y) - y;
}

long absl(long x) {
	long y = x >> 63;
	return (x ^ y) - y;
}