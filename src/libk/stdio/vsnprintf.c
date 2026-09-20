#include <libk/stdio.h>
#include <stdint.h>

int vsnprintf(char *__restrict str, size_t sz, const char *__restrict fmt, va_list arg) {
	// Temporary buffer large enough for kernel print operations
	char tmp_buf[256];
	va_list ap_copy;
	va_copy(ap_copy, arg);

	// Run your original vsprintf into temp buffer
	int total_chars = vsprintf(tmp_buf, fmt, ap_copy);
	va_end(ap_copy);

	if (sz == 0) {
		// No space to write anything, just return required length
		return total_chars;
	}

	size_t copy_len = ((size_t)total_chars < (sz - 1)) ? (size_t)total_chars : (sz - 1);

	// Copy the valid portion
	for (size_t i = 0; i < copy_len; i++) {
		str[i] = tmp_buf[i];
	}
	str[copy_len] = '\0';

	return total_chars;
}

int snprintf(char *__restrict str, size_t sz, const char *__restrict fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(str, sz, fmt, ap);
	va_end(ap);
	return ret;
}