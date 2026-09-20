/*
 * Host-side test for src/kernel/log/klog.c.
 *
 * tests/mock/kernel/spinlock.h replaces the real lock so this can run as an
 * unprivileged process.  Everything else is the production logger: real
 * timestamps (rdtsc), real formatting (libk vsnprintf), real ring buffer.
 *
 * Build and run with:  make tests
 */
#include <kernel/global.h> // bootinfo_t, required by tsc.h
#include <kernel/klog.h>
#include <kernel/tsc.h>
#include <stdio.h>
#include <string.h>

// Normally defined by src/kernel/global.c and kmain().
bootinfo_t _bi;
uint64_t _tsc_start = 0;
uint64_t _tsc_hz = 1000000000ULL; // 1 tick == 1 ns

static int failures;

#define CHECK(cond)                                                                            \
	do {                                                                                         \
		if (!(cond)) {                                                                            \
			failures++;                                                                             \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
		}                                                                                         \
	} while (0)

#define CAPTURE_BYTES (1 << 18)
static char captured[CAPTURE_BYTES];
static size_t captured_len;
static size_t counted_bytes;

static log_subscriber_t sub_capture;
static log_subscriber_t sub_count;
static log_subscriber_t sub_busy;

static size_t sink_capture(log_subscriber_t *sub, const char *data, size_t len) {
	(void)sub;
	if (captured_len + len + 1 > CAPTURE_BYTES) {
		printf("FAIL: capture buffer overflow\n");
		return len;
	}
	memcpy(captured + captured_len, data, len);
	captured_len += len;
	captured[captured_len] = '\0'; // keep the capture usable with strstr()
	return len;
}

static size_t sink_count(log_subscriber_t *sub, const char *data, size_t len) {
	(void)sub;
	(void)data;
	counted_bytes += len;
	return len;
}

// Simulates a sink whose hardware is permanently busy: it accepts nothing, so
// klog must fall behind and account for the gap.
static size_t sink_busy(log_subscriber_t *sub, const char *data, size_t len) {
	(void)sub;
	(void)data;
	(void)len;
	return 0;
}

static void test_immediate_delivery(void) {
	log_init();
	captured_len = 0;
	counted_bytes = 0;

	log_subscriber_register(&sub_capture, "capture", sink_capture, NULL);
	log_subscriber_register(&sub_count, "count", sink_count, NULL);

	printk("hello %s %d", "world", 42);

	CHECK(captured_len > 0);
	CHECK(captured_len == counted_bytes); // every sink sees the same stream
	CHECK(captured[0] == '[');						 // timestamp prefix
	CHECK(strstr(captured, "] hello world 42\n") != NULL);
	CHECK(sub_capture.lost == 0);
	CHECK(sub_count.lost == 0);
	// The ring holds exactly what the sinks received.
	CHECK(ringbuf_newest(log_ring()) == captured_len);

	// A message that already ends in '\n' must not gain a second one.
	captured_len = 0;
	counted_bytes = 0;
	printk("second line\n");
	CHECK(strstr(captured, "second line\n") != NULL);
	CHECK(strstr(captured, "second line\n\n") == NULL);
}

static void test_deferred_delivery(void) {
	size_t before = captured_len;

	log_set_immediate(false);
	printk("deferred line");
	CHECK(captured_len == before); // nothing is pushed while deferred

	log_subscribers_poll();
	CHECK(strstr(captured, "deferred line\n") != NULL);

	captured_len = 0;
	counted_bytes = 0;
	printk("force flushed");
	CHECK(captured_len == 0);
	log_force_flush();
	CHECK(strstr(captured, "force flushed\n") != NULL);
	log_set_immediate(true);
}

static void test_overwrite_accounting(void) {
	ringbuf_t *ring = log_ring();
	uint64_t start;
	uint64_t newest;
	uint64_t oldest;
	char tail[128];
	size_t n;
	unsigned i;

	// Only the busy sink stays active: its position freezes, so the writes
	// below will wrap the ring and force the "lost" path.
	sub_capture.active = false;
	sub_count.active = false;
	log_subscriber_register(&sub_busy, "busy", sink_busy, NULL);

	log_set_immediate(false);
	start = ringbuf_newest(ring);
	for (i = 0; i < 3000; i++) {
		printk("filler line %04u", i); // ~32 bytes each, way over 64 KiB total
	}
	log_subscribers_poll();
	log_set_immediate(true);

	newest = ringbuf_newest(ring);
	oldest = ringbuf_oldest(ring);

	CHECK(newest - start > LOG_RING_SIZE); // the ring really did wrap
	CHECK(oldest == newest - LOG_RING_SIZE);
	CHECK(sub_busy.pos == oldest);
	CHECK(sub_busy.lost == oldest - start);
	CHECK(ring->dropped == oldest); // every byte ever overwritten, since init

	// The newest line must survive, intact, at the tail of the ring.
	n = ringbuf_peek(ring, newest - 64, tail, sizeof(tail) - 1);
	CHECK(n == 64);
	tail[n] = '\0';
	CHECK(strstr(tail, "filler line 2999") != NULL);
}

int main(void) {
	test_immediate_delivery();
	test_deferred_delivery();
	test_overwrite_accounting();

	if (failures == 0) {
		printf("test_klog: all tests passed\n");
	} else {
		printf("test_klog: %d failure(s)\n", failures);
	}
	return failures != 0;
}
