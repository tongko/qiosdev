#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * PS/2 keyboard.  The ISR only pushes raw scancodes into a ring; decoding and
 * echoing happen in the poll below, in normal context, because drawing from an
 * interrupt handler races with whoever else owns the framebuffer.
 */

void kbd_init(void);

/*
 * Non-blocking: pops one decoded character.  Returns false when nothing is
 * queued or the queue holds no complete key yet.  Escape sequences for the
 * extended (arrow/function) keys are swallowed for now.
 */
bool kbd_getchar(char *out);

/*
 * Read the controller's output buffer once and queue the byte.  kbd_getchar()
 * no longer calls this - input is interrupt driven - but it stays available as
 * a diagnostic when a controller is not raising IRQ1.
 */
bool kbd_poll_raw(void);

// Interrupts the keyboard ISR has handled (0 means polling is carrying it).
uint32_t kbd_irq_count(void);

// Scancodes queued but not yet decoded.
uint32_t kbd_pending(void);

// False when there is no controller or no interrupt line, e.g. no PS/2 at all.
bool kbd_ready(void);
