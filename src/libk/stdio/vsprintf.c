/*
	vsprintf - sends formatted output to a string using an argument list passed
	 to it.
	Parameters:
	str		− This is the array of char elements where the resulting string is
			  to be stored.
	format	− This is the C string that contains the text to be written to the
			  str. It can optionally contain embedded format tags that are
			  replaced by the values specified in subsequent additional
			  arguments and are formatted as requested. Format tags prototype −
				%[flags][width][.precision][length]specifier
--------------------------------------------------------------------------------
	Specifier:

	c			Character
	d or i		Signed decimal integer
	e			Not supported in kernel mode.
	E			Not supported in kernel mode.
	f			Not supported in kernel mode.
	g			Not supported in kernel mode.
	G			Not supported in kernel mode.
	o			Not supported in kernel mode.
	s			String of characters
	u			Unsigned decimal integer
	x			Unsigned hexadecimal integer
	X			Unsigned hexadecimal integer (capital letters)
	p			Pointer address.
	n			Not supported in kernel mode.
	%			% Character
--------------------------------------------------------------------------------
	Flags:

	-			Left-justify within the given field width; Right justification
				is the default
				(see width sub-specifier).
	+			Not supported.
	(space)		Not supported.
	#			Used with x or X specifiers the value is preceded with 0x or 0X
				respectively for values different than zero. Not supported for e
				, E and f.
	0			Left-pads the number with zeroes (0) instead of spaces, where
				padding is specified (see width sub-specifier).
--------------------------------------------------------------------------------
	Width:

	(number)	Minimum number of characters to be printed. If the value to be
				printed is shorter than this number, the result is padded with
				blank spaces. The value is not truncated even if the result is
				larger.
	*			The width is not specified in the format string, but as an
				additional integer value argument preceding the argument that
				has to be formatted.
--------------------------------------------------------------------------------
	.precision:

	.number		For integer specifiers (d, i, x, X) − precision specifies the
				minimum number of digits to be written. If the value to be
				written is shorter than this number, the result is padded with
				leading zeros. The value is not truncated even if the result is
				longer. A precision of 0 means that no character is written for
				the value 0. Not supported e, E and f specifiers. For s − this
				is the maximum number of characters to be printed. By default
				all characters are printed until the ending null character is
				encountered. For c type − it has no effect. When no precision is
				specified, the default is 1. If the period is specified without
				an explicit value for precision, 0 is assumed. If precision has
				been specified, `Width` will be ignored.
	.*			The precision is not specified in the format string, but as an
				additional integer value argument preceding the argument that
				has to be formatted.
--------------------------------------------------------------------------------
	Length:

	h			The argument is interpreted as a short int or unsigned short int
				(only applies to integer specifiers − i, d, o, u, x and X).
	l			The argument is interpreted as a long int or unsigned long int
				for integer specifiers (i, d, o, u, x and X), and as a wide
				character or wide character string for specifiers c and s.
	L			he argument is interpreted as a long double (only applies to
				floating point specifiers − e, E, f, g and G).
*/
#include <libk/stdio.h>
#include <libk/stdlib.h>
#include <libk/string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DIGIT_STR(f) (char *)(f == 'X' ? "0123456789ABCDEF" : "0123456789abcdef")

#define PRINT_CHAR(C)                                                                                                  \
	do {                                                                                                                \
		*value++ = C;                                                                                                    \
		*value = '\0';                                                                                                   \
		i++;                                                                                                             \
	} while (0)

#define BASE(x)                                                                                                        \
	do {                                                                                                                \
		switch ((char)x) {                                                                                               \
		case 'p':                                                                                                        \
		case 'x':                                                                                                        \
		case 'X':                                                                                                        \
			base = 16;                                                                                                    \
			break;                                                                                                        \
		case 'o':                                                                                                        \
			base = 8;                                                                                                     \
			break;                                                                                                        \
		case 't':                                                                                                        \
			base = 2;                                                                                                     \
			break;                                                                                                        \
		default:                                                                                                         \
			base = 10;                                                                                                    \
			break;                                                                                                        \
		}                                                                                                                \
	} while (0)

typedef enum { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_LD } length_modifier_t;

#define FMTS_FLAGS_LEFTJUSTIFIED 0x01
#define FMTS_FLAGS_FORCESIGN 0x02
#define FMTS_FLAGS_FILLSPACE 0x04
#define FMTS_FLAGS_LEADSIGN 0x08
#define FMTS_FLAGS_PADZEROES 0x10

typedef struct format_specifier {
	char specifier;
	int flags;
	int fillWidth;
	int precision;
	length_modifier_t length;
} format_specifier_t;

static const char *_flags_string = "-+ #0";

static int print_number_ul(char *dest, unsigned long long value, format_specifier_t *format) {
	if (!format->precision && !value)
		return 0;

	char *p = dest;
	const char *str_digit = DIGIT_STR(format->specifier);
	int len = 0;
	char buf1[32] = {0};
	int base;
	bool has_sign = false;
	long long signed_val = (long long)value;

	BASE(format->specifier);

	switch (format->specifier) {
	case 'd':
	case 'i': {
		has_sign = (signed_val < 0) || (format->flags & FMTS_FLAGS_FORCESIGN);
		unsigned long long v;
		if (signed_val < 0)
			v = (unsigned long long)(-signed_val);
		else
			v = (unsigned long long)signed_val;

		if (format->length == LEN_HH) {
			len += itoa((int8_t)v, buf1, base, str_digit);
		} else if (format->length == LEN_H) {
			len += itoa((int16_t)v, buf1, base, str_digit);
		} else if (format->length == LEN_LL) {
			len += ltoa((long long)v, buf1, base, str_digit);
		} else if (format->length == LEN_L) {
			len += ltoa((long)v, buf1, base, str_digit);
		} else {
			len += itoa((int32_t)v, buf1, base, str_digit);
		}
	} break;
	case 'x':
	case 'X':
	case 'u':
	case 'p': {
		has_sign = false;
		if (format->length == LEN_HH) {
			len += utoa((uint8_t)value, buf1, base, str_digit);
		} else if (format->length == LEN_H) {
			len += utoa((uint16_t)value, buf1, base, str_digit);
		} else if (format->length == LEN_LL) {
			len += ultoa(value, buf1, base, str_digit);
		} else if (format->length == LEN_L) {
			len += ultoa((unsigned long)value, buf1, base, str_digit);
		} else {
			len += utoa((uint32_t)value, buf1, base, str_digit);
		}
	} break;
	default:
		buf1[0] = '\0';
	}

	if (format->flags & FMTS_FLAGS_PADZEROES) {
		format->precision = MAX(format->precision, format->fillWidth);
		format->fillWidth = 0;
	}

	int max_width = MAX(format->precision, format->fillWidth);
	max_width = MAX(max_width, len);
	int paddzero = MAX(format->precision - len, 0);
	paddzero = MAX(paddzero, 0);
	int to_be_minus = MAX(len, format->precision);
	int paddspace = MAX(format->fillWidth - to_be_minus, 0);
	paddspace = MAX(paddspace, 0);
	int ret_val = max_width;
	int printed = 0;

	if (len > format->precision && len > format->fillWidth) {
		if (has_sign) {
			*p++ = signed_val < 0 ? '-' : '+';
			printed++;
			ret_val++;
		} else if (format->flags & FMTS_FLAGS_LEADSIGN) {
			*p++ = '0';
			*p++ = format->specifier;
			printed += 2;
			ret_val += 2;
		}
		for (int i = 0; i < max_width; i++) {
			*p++ = buf1[i];
		}
	} else if (format->flags & FMTS_FLAGS_LEFTJUSTIFIED) {
		if (has_sign) {
			*p++ = signed_val < 0 ? '-' : '+';
			printed++;
			ret_val++;
		} else if (format->flags & FMTS_FLAGS_LEADSIGN) {
			*p++ = '0';
			*p++ = format->specifier;
			printed += 2;
			ret_val += 2;
		}
		for (int i = 0; i < paddzero; i++) {
			*p++ = '0';
		}
		for (int i = 0; i < len; i++) {
			*p++ = buf1[i];
		}
		for (int i = 0; i < (paddspace - printed); i++) {
			*p++ = ' ';
		}
	} else {
		if (has_sign) {
			printed++;
		} else if (format->flags & FMTS_FLAGS_LEADSIGN) {
			printed += 2;
		}
		for (int i = 0; i < (paddspace - printed); i++) {
			*p++ = ' ';
		}
		if (has_sign) {
			*p++ = signed_val < 0 ? '-' : '+';
			ret_val++;
		} else if (format->flags & FMTS_FLAGS_LEADSIGN) {
			*p++ = '0';
			*p++ = format->specifier;
			ret_val += 2;
		}
		for (int i = 0; i < paddzero; i++) {
			*p++ = '0';
		}
		for (int i = 0; i < len; i++) {
			*p++ = buf1[i];
		}
	}
	*p = '\0';
	return ret_val;
}

static unsigned int print_string(char *dest, const char *str, format_specifier_t *format) {
	size_t len = strlen(str);
	size_t c_to_print = MIN(len, (unsigned int)format->precision);
	int i;
	if ((int)c_to_print >= format->fillWidth) {
		if (c_to_print == 0) {
			return 0;
		}
		for (i = 0; i < (int)c_to_print; i++) {
			*dest++ = *str++;
			*dest = '\0';
		}
		return c_to_print;
	}
	if (format->flags & FMTS_FLAGS_LEFTJUSTIFIED) {
		for (i = 0; i < format->fillWidth; i++) {
			*dest++ = i < (int)c_to_print ? *str++ : ' ';
			*dest = '\0';
		}
	} else {
		for (i = 0; i < format->fillWidth; i++) {
			*dest++ = i < format->fillWidth - (int)c_to_print ? ' ' : *str++;
			*dest = '\0';
		}
	}
	return format->fillWidth;
}

int vsprintf(char *__restrict str, const char *__restrict format, va_list arg) {
	format_specifier_t fmt_spcf = {'\0', 0, -1, -1, LEN_NONE};
	const char *ptr = format;
	char *value = str;
	unsigned int i = 0;
	char fsbuf[32] = {0};
	unsigned int idx = 0;

	while (*ptr != '\0') {
		if (*ptr != '%') {
			do {
				PRINT_CHAR(*ptr++);
			} while (*ptr != '%' && *ptr != '\0');
		} else {
			ptr++;
			fmt_spcf.specifier = '\0';
			fmt_spcf.flags = 0;
			fmt_spcf.fillWidth = fmt_spcf.precision = -1;
			fmt_spcf.length = LEN_NONE;

			/* Flags parser */
			while (strchr(_flags_string, *ptr)) {
				char f = *ptr;
				ptr++;
				if (f == '-')
					fmt_spcf.flags |= FMTS_FLAGS_LEFTJUSTIFIED;
				if (f == '+')
					fmt_spcf.flags |= FMTS_FLAGS_FORCESIGN;
				if (f == ' ')
					fmt_spcf.flags |= FMTS_FLAGS_FILLSPACE;
				if (f == '#')
					fmt_spcf.flags |= FMTS_FLAGS_LEADSIGN;
				if (f == '0')
					fmt_spcf.flags |= FMTS_FLAGS_PADZEROES;
			}

			/* Width */
			if (*ptr == '*') {
				fmt_spcf.fillWidth = va_arg(arg, int);
				ptr++;
			} else {
				idx = 0;
				while (ISDIGIT(*ptr)) {
					fsbuf[idx++] = *ptr;
					fsbuf[idx] = '\0';
					ptr++;
				}
				if (idx > 0) {
					fmt_spcf.fillWidth = atoi(fsbuf);
				}
			}

			/* Precision */
			if (*ptr == '.') {
				ptr++;
				if (*ptr == '*') {
					fmt_spcf.precision = va_arg(arg, int);
					ptr++;
				} else {
					idx = 0;
					while (ISDIGIT(*ptr)) {
						fsbuf[idx++] = *ptr;
						fsbuf[idx] = '\0';
						ptr++;
					}
					if (idx > 0) {
						fmt_spcf.precision = atoi(fsbuf);
					}
				}
			}

			/* ===== LENGTH MODIFIER PARSER (handles hh, h, l, ll) ===== */
			if (*ptr == 'h') {
				ptr++;
				if (*ptr == 'h') {
					fmt_spcf.length = LEN_HH;
					ptr++;
				} else {
					fmt_spcf.length = LEN_H;
				}
			} else if (*ptr == 'l') {
				ptr++;
				if (*ptr == 'l') {
					fmt_spcf.length = LEN_LL;
					ptr++;
				} else {
					fmt_spcf.length = LEN_L;
				}
			} else if (*ptr == 'L') {
				fmt_spcf.length = LEN_LD;
				ptr++;
			}

			fmt_spcf.specifier = *ptr;
			switch (*ptr++) {
			case 'd':
			case 'i':
			case 'u':
			case 'x':
			case 'X': {
				char num_buff[32] = {0};
				unsigned long long num_val = 0;
				long long signed_num = 0;

				if (fmt_spcf.specifier == 'd' || fmt_spcf.specifier == 'i') {
					// SIGNED path for %d / %i
					switch (fmt_spcf.length) {
					case LEN_LL:
						signed_num = va_arg(arg, long long);
						break;
					case LEN_L:
						signed_num = va_arg(arg, long);
						break;
					case LEN_H:
						signed_num = (int16_t)va_arg(arg, int);
						break;
					case LEN_HH:
						signed_num = (int8_t)va_arg(arg, int);
						break;
					default:
						signed_num = va_arg(arg, int);
						break;
					}
					num_val = (unsigned long long)signed_num;
				} else {
					// UNSIGNED path for %u %x %X
					switch (fmt_spcf.length) {
					case LEN_LL:
						num_val = va_arg(arg, unsigned long long);
						break;
					case LEN_L:
						num_val = va_arg(arg, unsigned long);
						break;
					case LEN_H:
						num_val = (uint16_t)va_arg(arg, unsigned int);
						break;
					case LEN_HH:
						num_val = (uint8_t)va_arg(arg, unsigned int);
						break;
					default:
						num_val = va_arg(arg, unsigned int);
						break;
					}
				}
				unsigned int x = print_number_ul(num_buff, num_val, &fmt_spcf);
				strcat(value, num_buff);
				value += x;
				i += x;
			} break;
			case 'p': {
				void *ptr_arg = va_arg(arg, void *);
				char num_buff[32] = {0};
				uintptr_t v = (uintptr_t)ptr_arg;
				fmt_spcf.flags |= FMTS_FLAGS_LEADSIGN;
				fmt_spcf.specifier = 'x';
				fmt_spcf.precision = -1;
				fmt_spcf.length = LEN_LL;
				unsigned int x = print_number_ul(num_buff, v, &fmt_spcf);
				strcat(value, num_buff);
				value += x;
				i += x;
			} break;
			case 'c': {
				int c = va_arg(arg, int);
				PRINT_CHAR(c);
			} break;
			case 's': {
				const char *s = va_arg(arg, const char *);
				unsigned int x = print_string(value, s, &fmt_spcf);
				value += x;
				i += x;
			} break;
			case '%':
				PRINT_CHAR('%');
				break;
			default:
				return -2;
			}
		}
	}
	return i;
}
