#pragma once

/*
 * klog - kernel log ring plus subscribers.
 *
 * printk() formats one line into a 64 KiB text ring (the "dmesg" store).  Every
 * registered subscriber is then handed the bytes it has not seen yet, so the
 * same stream can feed a serial port, a framebuffer console and a future
 * dmesg/`log` command without anyone fighting over a single read pointer.
 *
 * Layout:
 *   kernel/ringbuf.h    the ring itself (absolute positions, overwrite policy)
 *   kernel/klog.h/.c    timestamps, formatting, subscribers
 *   kernel/serial.h     COM1 driver + serial_log_write() sink
 */

#include <kernel/ringbuf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Text ring size.  64 KiB of formatted lines is plenty for bring-up. */
#define LOG_RING_SIZE (1 << 16)

/* Longest single formatted line, timestamp and trailing '\n' included. */
#define LOG_LINE_MAX 256

/* How many bytes a subscriber gets per callback. */
#define LOG_CHUNK_SIZE 128

typedef struct log_subscriber log_subscriber_t;

/*
 * Push len bytes into a sink.  Return how many bytes were accepted; returning
 * less than len means "I am busy", and klog retries the remainder on the next
 * poll.  A polled UART always returns len.
 */
typedef size_t (*log_sink_fn)(log_subscriber_t *sub, const char *data, size_t len);

struct log_subscriber {
	const char *name;	 // for debugging
	log_sink_fn write; // sink, e.g. serial_log_write()
	void *ctx;			 // sink private data
	uint64_t pos;		 // absolute position in the log stream
	uint64_t lost;		 // bytes missed because we fell behind
	bool active;
	log_subscriber_t *next; // intrusive list
};

/* Reset the ring and forget every subscriber.  Call once, early in kmain. */
void log_init(void);

/* Format one line into the log.  A trailing '\n' is added when missing. */
void printk(const char *fmt, ...);

/* Attach a sink: it receives everything written after this call. */
void log_subscriber_register(log_subscriber_t *sub, const char *name, log_sink_fn write, void *ctx);

/* Let every subscriber consume what is pending. */
void log_subscribers_poll(void);

/* Like poll, but ignores the "someone else is draining" guard.  For panic. */
void log_force_flush(void);

/* Log a line, flush it, disable interrupts and halt this CPU.  Never returns. */
void log_panic(const char *fmt, ...) __attribute__((noreturn));

/*
 * true (default): printk() pushes straight to the sinks, so serial output
 * appears immediately.  false: lines only accumulate in the ring until
 * log_subscribers_poll() runs; faster and better suited to interrupt context.
 */
void log_set_immediate(bool on);

/* Raw ring, e.g. for a future dmesg command. */
ringbuf_t *log_ring(void);
