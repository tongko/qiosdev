/*
 * Host-side unit tests for src/include/kernel/ringbuf.h.
 * Build and run with:  make tests
 */
#include <kernel/ringbuf.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                            \
	do {                                                                                         \
		if (!(cond)) {                                                                            \
			failures++;                                                                             \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
		}                                                                                         \
	} while (0)

static uint8_t storage[8];
static ringbuf_t rb;
static char out[32];

static void fresh(void) {
	memset(storage, 0, sizeof(storage));
	ringbuf_init(&rb, storage, sizeof(storage));
}

static void test_empty(void) {
	fresh();
	CHECK(ringbuf_newest(&rb) == 0);
	CHECK(ringbuf_oldest(&rb) == 0);
	CHECK(ringbuf_valid(&rb) == 0);
	CHECK(ringbuf_peek(&rb, 0, out, sizeof(out)) == 0);
}

static void test_basic(void) {
	fresh();
	CHECK(ringbuf_write(&rb, "abcdef", 6) == 6);
	CHECK(ringbuf_newest(&rb) == 6);
	CHECK(ringbuf_oldest(&rb) == 0);
	CHECK(ringbuf_valid(&rb) == 6);
	CHECK(ringbuf_peek(&rb, 0, out, sizeof(out)) == 6);
	CHECK(memcmp(out, "abcdef", 6) == 0);
	// peek must not consume anything
	CHECK(ringbuf_newest(&rb) == 6);
	CHECK(rb.dropped == 0);
}

static void test_wraparound(void) {
	fresh();
	ringbuf_write(&rb, "abcdef", 6);
	ringbuf_write(&rb, "ghij", 4); // bytes 6..9, wraps the array
	CHECK(ringbuf_newest(&rb) == 10);
	CHECK(ringbuf_oldest(&rb) == 2); // 10 - cap(8)
	CHECK(ringbuf_valid(&rb) == 8);
	CHECK(ringbuf_peek(&rb, 2, out, sizeof(out)) == 8);
	CHECK(memcmp(out, "cdefghij", 8) == 0);
	CHECK(rb.dropped == 2);

	// peek() does not clamp by itself: a reader that fell behind skips to
	// ringbuf_oldest() first.  That is the contract klog's drain relies on.
	{
		uint64_t reader_pos = 0;
		uint64_t oldest = ringbuf_oldest(&rb);

		CHECK(reader_pos < oldest);
		reader_pos = oldest; // "lost" += oldest - reader_pos
		CHECK(ringbuf_peek(&rb, reader_pos, out, sizeof(out)) == 8);
		CHECK(memcmp(out, "cdefghij", 8) == 0);
	}
}

static void test_wrap_crossing_read(void) {
	fresh();
	ringbuf_write(&rb, "abcdef", 6);
	ringbuf_write(&rb, "gh", 2); // wpos = 8, tail = 0
	// this read starts at index 6 and has to wrap to index 0
	CHECK(ringbuf_peek(&rb, 4, out, sizeof(out)) == 4);
	CHECK(memcmp(out, "efgh", 4) == 0);
}

static void test_independent_readers(void) {
	fresh();
	ringbuf_write(&rb, "0123456", 7);

	CHECK(ringbuf_peek(&rb, 0, out, 3) == 3);
	CHECK(memcmp(out, "012", 3) == 0);
	CHECK(ringbuf_peek(&rb, 4, out, sizeof(out)) == 3);
	CHECK(memcmp(out, "456", 3) == 0);
	// reader B advancing must not disturb reader A
	CHECK(ringbuf_peek(&rb, 0, out, 7) == 7);
	CHECK(memcmp(out, "0123456", 7) == 0);
}

static void test_write_larger_than_buffer(void) {
	fresh();
	CHECK(ringbuf_write(&rb, "0123456789ABC", 13) == 8); // only 8 bytes fit
	CHECK(ringbuf_newest(&rb) == 8);
	CHECK(ringbuf_oldest(&rb) == 0);
	CHECK(rb.dropped == 5);
	CHECK(ringbuf_peek(&rb, 0, out, sizeof(out)) == 8);
	CHECK(memcmp(out, "56789ABC", 8) == 0);
}

static void test_partial_consume(void) {
	fresh();
	ringbuf_write(&rb, "lo world", 8);
	CHECK(ringbuf_peek(&rb, 0, out, 3) == 3);
	CHECK(memcmp(out, "lo ", 3) == 0);
	CHECK(ringbuf_peek(&rb, 3, out, 100) == 5); // clamps to what is available
	CHECK(memcmp(out, "world", 5) == 0);
	// reading past the head yields nothing
	CHECK(ringbuf_peek(&rb, 8, out, sizeof(out)) == 0);
}

static void test_lapped_reader_accounting(void) {
	uint64_t reader_pos = 0;
	uint64_t oldest;
	uint64_t lost;

	fresh();
	ringbuf_write(&rb, "aaaa", 4); // wpos 4
	ringbuf_write(&rb, "bbbb", 4); // wpos 8
	CHECK(ringbuf_oldest(&rb) == 0);
	ringbuf_write(&rb, "cccc", 4); // wpos 12 -> tail 4, "aaaa" overwritten

	oldest = ringbuf_oldest(&rb);
	CHECK(oldest == 4);
	CHECK(reader_pos < oldest);
	lost = oldest - reader_pos;
	CHECK(lost == 4);
	reader_pos = oldest;

	CHECK(ringbuf_peek(&rb, reader_pos, out, sizeof(out)) == 8);
	CHECK(memcmp(out, "bbbbcccc", 8) == 0);
	CHECK(rb.dropped == 4);
}

int main(void) {
	test_empty();
	test_basic();
	test_wraparound();
	test_wrap_crossing_read();
	test_independent_readers();
	test_write_larger_than_buffer();
	test_partial_consume();
	test_lapped_reader_accounting();

	if (failures == 0) {
		printf("test_ringbuf: all tests passed\n");
	} else {
		printf("test_ringbuf: %d failure(s)\n", failures);
	}
	return failures != 0;
}
