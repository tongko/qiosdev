#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOG_RING_SIZE (1 << 16) // 64KB ring buffer, adjust as needed
#define LOG_MAX_MSG_LEN 256

// One log record stored inside ring buffer
typedef struct {
	double timestamp; // boot uptime (TSC based)
	char msg[LOG_MAX_MSG_LEN];
} log_record_t;

// Forward declare subscriber
typedef struct log_subscriber log_subscriber_t;

// Subscriber callback type: invoked when there is new log line
// Return true if consumed, false to retry later
typedef bool (*log_subscriber_cb_t)(log_subscriber_t *sub, const log_record_t *rec);

struct log_subscriber {
	log_subscriber_cb_t callback;
	size_t read_pos; // OWN read pointer (per-subscriber)
	bool active;
};

// Max number of log output channels (serial, vga, etc.)
#define LOG_MAX_SUBSCRIBERS 4

// Initialize ring buffer subsystem (call once in mm_init / early kernel init)
void log_init(void);

// Kernel's printk: write one log entry into ring buffer
void printk(const char *fmt, ...);

// Register a new subscriber (serial / vga console)
int log_subscriber_register(log_subscriber_t *sub, log_subscriber_cb_t cb);

// Poll ALL subscribers: let them consume pending log entries
void log_subscribers_poll(void);