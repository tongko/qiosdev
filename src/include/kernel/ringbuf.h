#pragma once

/*
 * ringbuf - single-producer / multiple-consumer byte ring buffer.
 *
 * Instead of wrapping read pointers around the storage array, the buffer
 * hands every consumer an *absolute* position into an endless byte stream.
 * Absolute positions never wrap, so bookkeeping is trivial:
 *
 *     [0 .. tail)       overwritten, gone forever
 *     [tail .. wpos)    still readable
 *     [wpos .. )        not written yet
 *
 * A consumer owns its own uint64_t position and calls ringbuf_peek() to copy
 * the bytes it is about to consume.  When the producer laps a consumer,
 * ringbuf_oldest() jumps forward and the consumer can count the skipped bytes
 * as "lost" instead of printing torn data.  That is exactly what a kernel log
 * wants: the newest output always wins, and the gap is reported.
 *
 * Producer side is not reentrant: serialize producers with a lock (see
 * klog.c).  Consumers are lock-free and independent of each other.
 *
 * cap must be a power of two (the modulo becomes a mask).
 */

#include <libk/string.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint8_t *buf;		// storage, cap bytes
	size_t cap;			// capacity in bytes, power of two
	uint64_t wpos;		// absolute position of the write head
	uint64_t tail;		// absolute position of the oldest valid byte
	uint64_t dropped; // bytes discarded to overwrite, since init
} ringbuf_t;

static inline void ringbuf_init(ringbuf_t *rb, void *storage, size_t cap) {
	rb->buf = (uint8_t *)storage;
	rb->cap = cap;
	rb->wpos = 0;
	rb->tail = 0;
	rb->dropped = 0;
}

/* Absolute position one past the last valid byte. */
static inline uint64_t ringbuf_newest(ringbuf_t *rb) {
	return __atomic_load_n(&rb->wpos, __ATOMIC_ACQUIRE);
}

/* Absolute position of the oldest byte that is still valid. */
static inline uint64_t ringbuf_oldest(ringbuf_t *rb) {
	return __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);
}

/* Number of valid bytes currently stored (diagnostics only). */
static inline size_t ringbuf_valid(const ringbuf_t *rb) {
	return (size_t)(rb->wpos - rb->tail);
}

/*
 * Append len bytes.  On overflow the oldest bytes are discarded and counted in
 * rb->dropped, so the tail of the new data always survives.  Returns len.
 */
static inline size_t ringbuf_write(ringbuf_t *rb, const void *src, size_t len) {
	const uint8_t *in = (const uint8_t *)src;
	size_t idx;
	size_t first;
	uint64_t wpos;
	uint64_t tail;

	if (rb->buf == NULL || rb->cap == 0) {
		return 0;
	}

	if (len > rb->cap) {
		// A single write bigger than the buffer: only the last cap bytes live.
		rb->dropped += len - rb->cap;
		in += len - rb->cap;
		len = rb->cap;
	}

	idx = (size_t)(rb->wpos & (uint64_t)(rb->cap - 1));
	first = rb->cap - idx;
	if (first > len) {
		first = len;
	}

	memcpy(rb->buf + idx, in, first);
	if (len > first) {
		memcpy(rb->buf, in + first, len - first);
	}

	wpos = rb->wpos + len;
	tail = (wpos > rb->cap) ? wpos - rb->cap : 0;
	if (tail > rb->tail) {
		rb->dropped += tail - rb->tail;
	}

	rb->tail = tail;
	// Release: the payload must be visible before consumers see the new head.
	__atomic_store_n(&rb->wpos, wpos, __ATOMIC_RELEASE);
	return len;
}

/*
 * Copy up to max bytes starting at absolute position pos into dst, without
 * consuming anything.  Returns the number of bytes copied, 0 when pos has
 * caught up with the write head.
 *
 * pos must already be clamped to >= ringbuf_oldest().  A producer that runs
 * while this copy is in flight can still overwrite what we are reading; callers
 * that care re-check ringbuf_oldest() afterwards (see klog.c).
 */
static inline size_t ringbuf_peek(ringbuf_t *rb, uint64_t pos, void *dst, size_t max) {
	uint8_t *out = (uint8_t *)dst;
	uint64_t newest = __atomic_load_n(&rb->wpos, __ATOMIC_ACQUIRE);
	size_t avail;
	size_t idx;
	size_t first;

	if (max == 0 || pos >= newest) {
		return 0;
	}

	avail = (size_t)(newest - pos);
	if (avail > max) {
		avail = max;
	}

	idx = (size_t)(pos & (uint64_t)(rb->cap - 1));
	first = rb->cap - idx;
	if (first > avail) {
		first = avail;
	}

	memcpy(out, rb->buf + idx, first);
	if (avail > first) {
		memcpy(out + first, rb->buf, avail - first);
	}
	return avail;
}
