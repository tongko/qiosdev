#pragma once

/*
 * Polled 16550 UART on COM1.  No interrupts: the kernel can print from any
 * context, including before the IDT is set up, which is exactly when early
 * bring-up logging matters most.
 */

#include <kernel/klog.h>
#include <stddef.h>
#include <stdint.h>

#define SERIAL_COM1 0x3F8

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *data, size_t len);

/* klog sink: same as serial_write, plus LF -> CRLF translation. */
size_t serial_log_write(log_subscriber_t *sub, const char *data, size_t len);

/* Bring up COM1 and register it as a log sink (call after log_init()). */
void serial_log_init(void);
