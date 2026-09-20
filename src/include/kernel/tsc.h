#pragma once

#include <kernel/global.h>
#include <stddef.h>
#include <stdint.h>

extern uint64_t _tsc_start;
extern uint64_t _tsc_hz;

static inline uint64_t rdtsc(void) {
	uint32_t lo, hi;

	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));

	// Cast before shifting: hi is 32 bit, so "hi << 32" is undefined and
	// silently yields 0, throwing away the high half of the counter.
	return ((uint64_t)hi << 32) | lo;
}

// Nanoseconds since the TSC was sampled at boot:
// uint64_t ns   = get_boot_time();
// uint64_t sec  = ns / 1000000000ULL;
// uint64_t usec = (ns % 1000000000ULL) / 1000ULL;
static inline uint64_t get_boot_time(void) {
	uint64_t hz = _tsc_hz;
	uint64_t diff;

	if (hz == 0) {
		return 0; // TSC was never calibrated
	}

	diff = rdtsc() - _tsc_start;
	// Split the division on purpose: diff * 1e9 overflows uint64 after a few
	// seconds (about 6 s at 3 GHz) and would wrap every timestamp.
	return (diff / hz) * 1000000000ULL + ((diff % hz) * 1000000000ULL) / hz;
}
