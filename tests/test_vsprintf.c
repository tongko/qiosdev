#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

// include your vsprintf source
int vsprintf(char *__restrict str, const char *__restrict format, va_list arg);

#define BUF_SZ 128

static void test_case(const char *fmt, ...)
{
    char our_buf[BUF_SZ];
    char ref_buf[BUF_SZ];

    va_list ap1, ap2;
    va_start(ap1, fmt);
    va_copy(ap2, ap1);

    vsprintf(our_buf, fmt, ap1);
    vsnprintf(ref_buf, BUF_SZ, fmt, ap2);

    va_end(ap1);
    va_end(ap2);

    if (strcmp(our_buf, ref_buf) != 0)
    {
        fprintf(stderr, "FAIL: fmt=\"%s\"\n", fmt);
        fprintf(stderr, "  our:  |%s|\n", our_buf);
        fprintf(stderr, "  ref:  |%s|\n", ref_buf);
        assert(0);
    }
    else
    {
        printf("OK: %s → |%s|\n", fmt, our_buf);
    }
}

int main(void)
{
    // basic
    test_case("Hello %s", "QIOS");
    test_case("int %d", 123);
    test_case("neg %d", -456);

    // long long / %llu %lld (the one you just added!)
    test_case("llu = %llu", 0x123456789ABCDEFULL);
    test_case("lld = %lld", -123456789012345LL);

    // hex
    test_case("hex %x", 0xdead);
    test_case("hexX %X", 0xDEAD);
    test_case("ll hex %llx", 0xFFFF000012340000ULL);

    // pointer
    uint64_t dummy = 0x100000;
    test_case("ptr %p", &dummy);

    // width / padding
    test_case("width %10d", 42);
    test_case("zero pad %08x", 0x123);

    printf("\nAll tests passed!\n");
    return 0;
}