#pragma once

/*
 * x86 port I/O and a couple of CPU helpers.
 * Everything here is static inline so it needs no libk symbol.
 */

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
	uint8_t value;

	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

/* Write to the unused port 0x80 to waste a few bus cycles (legacy delay). */
static inline void io_wait(void) {
	outb(0x80, 0);
}

/* Hint the CPU that we are spinning in a lock loop. */
static inline void cpu_relax(void) {
	__asm__ volatile("pause");
}

/* Stop this CPU forever.  Interrupts are disabled first so nothing can
 * wake us up again.  Callers keep control until their next hlt. */
static inline void halt(void) {
	__asm__ volatile("cli; hlt");
}
