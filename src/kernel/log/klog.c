// log.c
#include "klog.h"
#include "tsc.h" // your TSC timestamp function double get_boot_time(void);
#include <stdarg.h>
#include <string.h>

static log_record_t log_ring[LOG_RING_SIZE];
static size_t write_pos = 0;
static log_subscriber_t subscribers[LOG_MAX_SUBSCRIBERS];
static size_t subscriber_count = 0;

// simple spinlock for writer (single producer, ready for SMP later)
static spinlock_t log_lock;

void log_init(void) {
	memset(log_ring, 0, sizeof(log_ring));
	write_pos = 0;
	memset(subscribers, 0, sizeof(subscribers));
	subscriber_count = 0;
	spinlock_init(&log_lock);
}

static void log_append_record(const char *msg) {
	spinlock_lock(&log_lock);

	log_record_t *rec = &log_ring[write_pos % LOG_RING_SIZE];
	rec->timestamp = get_boot_time();
	strncpy(rec->msg, msg, LOG_MAX_MSG_LEN - 1);
	rec->msg[LOG_MAX_MSG_LEN - 1] = '\0';

	write_pos++;

	spinlock_unlock(&log_lock);
}

void printk(const char *fmt, ...) {
	char buf[LOG_MAX_MSG_LEN];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	log_append_record(buf);
}

int log_subscriber_register(log_subscriber_t *sub, log_subscriber_cb cb) {
	if (subscriber_count >= LOG_MAX_SUBSCRIBERS)
		return -1;

	sub->callback = cb;
	sub->read_pos = write_pos; // start reading from current write point
	sub->active = true;

	subscribers[subscriber_count++] = *sub;
	return 0;
}

void log_subscribers_poll(void) {
	for (size_t i = 0; i < subscriber_count; i++) {
		log_subscriber_t *s = &subscribers[i];
		if (!s->active)
			continue;

		// while subscriber read position is behind writer
		while (s->read_pos < write_pos) {
			size_t idx = s->read_pos % LOG_RING_SIZE;
			log_record_t *rec = &log_ring[idx];

			bool consumed = s->callback(s, rec);
			if (!consumed) {
				// channel busy (e.g. serial TX buffer full), stop for now, retry next
				// poll
				break;
			}
			s->read_pos++;
		}
	}
}
