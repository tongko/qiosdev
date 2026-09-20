#pragma once

/*
 * Ticket spinlock.
 *
 * A ticket lock hands out FIFO order, so it stays fair under contention and
 * needs no compare-and-swap retry loop.  __atomic_* builtins compile to plain
 * lock xadd / mov on x86_64, no libatomic needed.
 *
 * Rule of thumb for kernel logging:
 *   - a lock that can be taken from an interrupt handler must be taken with
 *     spinlock_lock_irqsave(): otherwise the handler spins forever on a lock
 *     held by the very context it interrupted;
 *   - use spinlock_trylock() when the caller must never block (nested drains,
 *     panic paths).
 */

#include <kernel/io.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint32_t next;	 // next ticket to hand out
	uint32_t owner; // ticket currently allowed in
} spinlock_t;

#define SPINLOCK_INIT {0, 0}

static inline void spinlock_init(spinlock_t *lock) {
	lock->next = 0;
	lock->owner = 0;
}

static inline void spinlock_lock(spinlock_t *lock) {
	uint32_t ticket = __atomic_fetch_add(&lock->next, 1, __ATOMIC_RELAXED);

	while (__atomic_load_n(&lock->owner, __ATOMIC_ACQUIRE) != ticket) {
		cpu_relax();
	}
}

static inline void spinlock_unlock(spinlock_t *lock) {
	__atomic_fetch_add(&lock->owner, 1, __ATOMIC_RELEASE);
}

/* Non-blocking acquire: returns false when somebody else holds the lock. */
static inline bool spinlock_trylock(spinlock_t *lock) {
	uint32_t owner = __atomic_load_n(&lock->owner, __ATOMIC_RELAXED);
	uint32_t next = __atomic_load_n(&lock->next, __ATOMIC_RELAXED);

	if (owner != next) {
		return false;
	}
	return __atomic_compare_exchange_n(&lock->next, &next, next + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

/* Save RFLAGS (interrupt flag included), then disable interrupts. */
static inline uint64_t irq_save(void) {
	uint64_t flags;

	__asm__ volatile("pushfq\n\tpopq %0\n\tcli" : "=r"(flags) : : "memory");
	return flags;
}

static inline void irq_restore(uint64_t flags) {
	__asm__ volatile("pushq %0\n\tpopfq" : : "r"(flags) : "memory", "cc");
}

static inline void spinlock_lock_irqsave(spinlock_t *lock, uint64_t *flags) {
	*flags = irq_save();
	spinlock_lock(lock);
}

static inline void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags) {
	spinlock_unlock(lock);
	irq_restore(flags);
}
