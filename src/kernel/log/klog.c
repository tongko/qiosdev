// klog.c - kernel log ring, timestamps and subscribers.
#include <kernel/io.h>
#include <kernel/klog.h>
#include <kernel/spinlock.h>
#include <kernel/tsc.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <stdarg.h>

// The ring lives in .bss; the bootloader zeroes it, so log_init() only resets
// the indices instead of memset()ing 64 KiB on every boot.
static uint8_t _log_storage[LOG_RING_SIZE] __attribute__((aligned(64)));
static ringbuf_t _log_buf;

static log_subscriber_t *_log_subs;
static spinlock_t _log_lock = SPINLOCK_INIT;	 // serializes producers
static spinlock_t _poll_lock = SPINLOCK_INIT; // taken with trylock only
static bool _log_immediate = true;

void log_init(void) {
	ringbuf_init(&_log_buf, _log_storage, sizeof(_log_storage));
	_log_subs = NULL;
	_log_immediate = true;
	spinlock_init(&_log_lock);
	spinlock_init(&_poll_lock);
}

static uint64_t log_timestamp_ns(void) {
	uint64_t hz = _tsc_hz;
	uint64_t diff;

	if (hz == 0) {
		return 0; // TSC not calibrated: log without a timestamp rather than #DE
	}

	diff = rdtsc() - _tsc_start;
	// Split the division: diff * 1e9 overflows uint64 after a few seconds
	// (about 6 s at 3 GHz) and would corrupt every timestamp.
	return (diff / hz) * 1000000000ULL + ((diff % hz) * 1000000000ULL) / hz;
}

static void log_write_bytes(const char *data, size_t len, bool locking) {
	uint64_t flags = 0;

	if (_log_buf.buf == NULL) {
		return; // printk() before log_init()
	}

	if (locking) {
		spinlock_lock_irqsave(&_log_lock, &flags);
	}
	ringbuf_write(&_log_buf, data, len);
	if (locking) {
		spinlock_unlock_irqrestore(&_log_lock, flags);
	}
}

// Format "[ seconds.microseconds] message\n" and append it to the ring.
// locking == false is only used by the panic path, which must not block on a
// lock that an interrupted context may be holding.
static void log_vemit(const char *fmt, va_list ap, bool locking) {
	char line[LOG_LINE_MAX];
	uint64_t ns = log_timestamp_ns();
	size_t used;
	int n;
	int m;

	n = snprintf(line,
					 sizeof(line),
					 "[%5llu.%06llu] ",
					 (unsigned long long)(ns / 1000000000ULL),
					 (unsigned long long)((ns % 1000000000ULL) / 1000ULL));
	if (n < 0) {
		n = 0;
	}
	used = (size_t)n;
	if (used > sizeof(line) - 1) {
		used = sizeof(line) - 1;
	}

	m = vsnprintf(line + used, sizeof(line) - used, fmt, ap);
	if (m < 0) {
		m = 0;
	}
	used += (size_t)m;
	if (used > sizeof(line) - 1) {
		used = sizeof(line) - 1;
	}

	if (used == 0 || line[used - 1] != '\n') {
		if (used >= sizeof(line) - 1) {
			used = sizeof(line) - 2; // truncate the tail to keep the newline
		}
		line[used++] = '\n';
	}

	log_write_bytes(line, used, locking);
}

void printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	log_vemit(fmt, ap, true);
	va_end(ap);

	if (_log_immediate) {
		log_subscribers_poll();
	}
}

void log_subscriber_register(log_subscriber_t *sub, const char *name, log_sink_fn write, void *ctx) {
	uint64_t flags;

	if (sub == NULL) {
		return;
	}

	spinlock_lock_irqsave(&_log_lock, &flags);
	sub->name = name;
	sub->write = write;
	sub->ctx = ctx;
	sub->pos = ringbuf_newest(&_log_buf); // only see what comes next
	sub->lost = 0;
	sub->active = true;
	sub->next = _log_subs;
	_log_subs = sub;
	spinlock_unlock_irqrestore(&_log_lock, flags);
}

static void log_drain(log_subscriber_t *sub) {
	char chunk[LOG_CHUNK_SIZE];

	for (;;) {
		uint64_t oldest = ringbuf_oldest(&_log_buf);
		uint64_t newest;
		size_t n;
		size_t done;

		if (sub->pos < oldest) {
			// The producer lapped us: skip to the oldest valid byte and
			// account for the gap instead of printing stale bytes.
			sub->lost += oldest - sub->pos;
			sub->pos = oldest;
		}

		newest = ringbuf_newest(&_log_buf);
		if (sub->pos >= newest) {
			break;
		}

		n = ringbuf_peek(&_log_buf, sub->pos, chunk, sizeof(chunk));
		if (n == 0) {
			break;
		}

		// A slow sink can let the producer wrap around while we copy.  Drop
		// this chunk rather than pushing torn data to the console.
		if (sub->pos < ringbuf_oldest(&_log_buf)) {
			continue;
		}

		done = (sub->write != NULL) ? sub->write(sub, chunk, n) : n;
		sub->pos += done;
		if (done < n) {
			break; // sink busy: keep the remainder for the next poll
		}
	}
}

void log_subscribers_poll(void) {
	log_subscriber_t *sub;

	// trylock, never block: if an interrupt handler logs while the context it
	// interrupted is draining, the handler must not wait for it.  Its bytes
	// stay in the ring and the outer poll picks them up afterwards.
	if (!spinlock_trylock(&_poll_lock)) {
		return;
	}

	for (sub = _log_subs; sub != NULL; sub = sub->next) {
		if (sub->active) {
			log_drain(sub);
		}
	}

	spinlock_unlock(&_poll_lock);
}

void log_force_flush(void) {
	log_subscriber_t *sub;

	for (sub = _log_subs; sub != NULL; sub = sub->next) {
		if (sub->active) {
			log_drain(sub);
		}
	}
}

void log_panic(const char *fmt, ...) {
	va_list ap;

	// No locking here on purpose: we may have been called from an interrupt
	// handler that interrupted the lock holder.  Corrupting the ring beats
	// hanging forever.
	va_start(ap, fmt);
	log_vemit(fmt, ap, false);
	va_end(ap);

	log_force_flush();

	for (;;) {
		halt();
	}
}

void log_set_immediate(bool on) {
	_log_immediate = on;
}

ringbuf_t *log_ring(void) {
	return &_log_buf;
}
