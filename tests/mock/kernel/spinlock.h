#pragma once

/*
 * Host-test stand-in for kernel/spinlock.h.
 *
 * tests/ is placed before src/include on the include path, so unit tests link
 * against no-op locks instead of the real ones: taking a real kernel lock would
 * execute cli/sti/hlt, which is not allowed in a user process.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint32_t next;
	uint32_t owner;
} spinlock_t;

#define SPINLOCK_INIT                                                                          \
	{                                                                                           \
		0, 0                                                                                     \
	}

static inline void spinlock_init(spinlock_t *lock) {
	lock->next = 0;
	lock->owner = 0;
}

static inline void spinlock_lock(spinlock_t *lock) {
	(void)lock;
}

static inline void spinlock_unlock(spinlock_t *lock) {
	(void)lock;
}

static inline bool spinlock_trylock(spinlock_t *lock) {
	(void)lock;
	return true;
}

static inline void spinlock_lock_irqsave(spinlock_t *lock, uint64_t *flags) {
	(void)lock;
	*flags = 0;
}

static inline void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags) {
	(void)lock;
	(void)flags;
}
