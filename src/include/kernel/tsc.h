#pragma once

#include <kernel/global.h>
#include <stddef.h>
#include <stdint.h>

extern uint64_t _tsc_start;
extern uint64_t _tsc_hz;

static inline uint64_t rdtsc(void) {
	uint32_t lo, hi;

	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));

	return (uint64_t)(hi << 32) | lo;
}

// Caller should get the fraction by:
// uint64_t ns = get_boot_ns();
// uint64_t sec  = ns / 1000000000ULL;
// uint64_t frac = (ns % 1000000000ULL) / 1000ULL;
// serial_printf("[%llu.%06llu] %s\n", sec, frac, msg);
// this will print something like [0.000123]
static inline uint64_t get_boot_time(void) {
	uint64_t now = rdtsc();
	uint64_t diff = now - _bi.tsc_start;
	// diff * 1e9 / freq → nanoseconds
	return (diff * 1000000000ULL) / _bi.tsc_hz;
}
