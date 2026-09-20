// serial.c - polled 16550 UART on COM1, used as the first klog sink.
#include <kernel/io.h>
#include <kernel/klog.h>
#include <kernel/serial.h>

#define UART_DATA 0 // RBR / THR
#define UART_IER 1	// interrupt enable
#define UART_FCR 2	// FIFO control
#define UART_LCR 3	// line control
#define UART_MCR 4	// modem control
#define UART_LSR 5	// line status

#define LSR_THR_EMPTY 0x20

static log_subscriber_t serial_sub;

static int serial_tx_ready(void) {
	return (inb(SERIAL_COM1 + UART_LSR) & LSR_THR_EMPTY) != 0;
}

void serial_init(void) {
	outb(SERIAL_COM1 + UART_IER, 0x00); // we poll, so no UART interrupts
	outb(SERIAL_COM1 + UART_LCR, 0x80); // DLAB on to program the divisor
	outb(SERIAL_COM1 + UART_DATA, 0x01); // divisor low  = 1
	outb(SERIAL_COM1 + UART_IER, 0x00);  // divisor high = 0  -> 115200 baud
	outb(SERIAL_COM1 + UART_LCR, 0x03); // 8 data bits, no parity, 1 stop bit
	outb(SERIAL_COM1 + UART_FCR, 0xC7); // enable + clear FIFOs, 14 byte trigger
	outb(SERIAL_COM1 + UART_MCR, 0x0B); // DTR, RTS, OUT2
}

void serial_putc(char c) {
	while (!serial_tx_ready()) {
		cpu_relax();
	}
	outb(SERIAL_COM1 + UART_DATA, (uint8_t)c);
}

void serial_write(const char *data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (data[i] == '\n') {
			serial_putc('\r'); // terminals want CRLF
		}
		serial_putc(data[i]);
	}
}

size_t serial_log_write(log_subscriber_t *sub, const char *data, size_t len) {
	(void)sub;
	serial_write(data, len);
	return len; // a polled UART never backpressures
}

void serial_log_init(void) {
	serial_init();
	log_subscriber_register(&serial_sub, "com1", serial_log_write, NULL);
}
