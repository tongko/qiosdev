// kbd.c - PS/2 keyboard, interrupt driven.
//
// The IRQ1 handler reads one scancode out of the 8042 and queues it;
// kbd_getchar() decodes the queue in normal context, where rendering is safe.
// The controller config byte's bit 0 is the first port's interrupt enable and
// bit 4 disables its clock, so both matter: clearing bit 0 (rather than
// setting it) is a keyboard that produces scancodes but never raises IRQ1.
// Every wait is bounded: a machine without a PS/2 controller must not hang.
#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/devices/kbd.h>
#include <kernel/idt.h>
#include <kernel/io.h>
#include <kernel/klog.h>
#include <kernel/ringbuf.h>
#include <libk/string.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_CMD_PORT 0x64

#define KBD_STATUS_OUT_FULL 0x01
#define KBD_STATUS_IN_FULL 0x02

#define KBD_IRQ 1
#define KBD_VECTOR 33 // IRQ_VECTOR_MIN + 1

#define KBD_QUEUE_SIZE 64

// Scancode set 1.  Letters live here in lower case only; the shifted map holds
// just the punctuation that differs, and modifier keys are handled by name.
static const char kbd_map[128] = {
	[0x01] = 27, // Esc
	[0x02] = '1', [0x03] = '2', [0x04] = '3',	 [0x05] = '4', [0x06] = '5',	[0x07] = '6',	[0x08] = '7', [0x09] = '8',
	[0x0A] = '9', [0x0B] = '0', [0x0C] = '-',	 [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t', [0x10] = 'q', [0x11] = 'w',
	[0x12] = 'e', [0x13] = 'r', [0x14] = 't',	 [0x15] = 'y', [0x16] = 'u',	[0x17] = 'i',	[0x18] = 'o', [0x19] = 'p',
	[0x1A] = '[', [0x1B] = ']', [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's',	[0x20] = 'd',	[0x21] = 'f', [0x22] = 'g',
	[0x23] = 'h', [0x24] = 'j', [0x25] = 'k',	 [0x26] = 'l', [0x27] = ';',	[0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
	[0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',	 [0x2F] = 'v', [0x30] = 'b',	[0x31] = 'n',	[0x32] = 'm', [0x33] = ',',
	[0x34] = '.', [0x35] = '/', [0x37] = '*',	 [0x39] = ' ',
};

static const char kbd_map_shift[128] = {
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^', [0x08] = '&',
	[0x09] = '*', [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+', [0x1A] = '{', [0x1B] = '}',
	[0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

static uint8_t kbd_storage[KBD_QUEUE_SIZE];
static ringbuf_t kbd_ring;
static uint64_t kbd_read_pos;
static bool kbd_shift;
static bool kbd_caps;
static bool kbd_extended;
static bool kbd_live;
static uint32_t kbd_irqs;
static bool kbd_reported;

static bool kbd_wait_input_clear(void) {
	for (uint32_t i = 0; i < 100000; i++) {
		if ((inb(KBD_STATUS_PORT) & KBD_STATUS_IN_FULL) == 0) {
			return true;
		}
		cpu_relax();
	}
	return false;
}

static bool kbd_wait_output_full(void) {
	for (uint32_t i = 0; i < 100000; i++) {
		if ((inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) != 0) {
			return true;
		}
		cpu_relax();
	}
	return false;
}

static bool kbd_write_cmd(uint8_t command) {
	if (!kbd_wait_input_clear()) {
		return false;
	}
	outb(KBD_CMD_PORT, command);
	return true;
}

static bool kbd_write_data(uint8_t value) {
	if (!kbd_wait_input_clear()) {
		return false;
	}
	outb(KBD_DATA_PORT, value);
	return true;
}

// Throw away anything already queued: several controller commands answer with
// 0xFA, and a stray ACK would otherwise be mistaken for the next reply.
static void kbd_drain(uint32_t max) {
	for (uint32_t i = 0; i < max && (inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) != 0; i++) {
		(void)inb(KBD_DATA_PORT);
	}
}

static bool kbd_read_data(uint8_t *out) {
	if (!kbd_wait_output_full()) {
		return false;
	}
	*out = inb(KBD_DATA_PORT);
	return true;
}

// Runs in interrupt context with interrupts already off: read the byte and
// leave.  The dispatcher acknowledges the controller, and rendering happens in
// kbd_getchar() instead.
static void kbd_isr(void *ctx, interrupt_frame_t *frame) {
	uint8_t scancode;

	(void)ctx;
	(void)frame;

	// Read only when the output buffer actually holds a byte; a spurious entry
	// would otherwise read the previous scancode again and duplicate a key.
	if ((inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) == 0) {
		return;
	}

	scancode = inb(KBD_DATA_PORT);
	kbd_irqs++;
	ringbuf_write(&kbd_ring, &scancode, 1);
}

bool kbd_poll_raw(void) {
	uint8_t scancode;

	if ((inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) == 0) {
		return false;
	}

	scancode = inb(KBD_DATA_PORT);
	ringbuf_write(&kbd_ring, &scancode, 1);
	return true;
}

uint32_t kbd_irq_count(void) {
	return kbd_irqs;
}

static char kbd_decode(uint8_t scancode) {
	if (scancode == 0xE0) { // extended prefix: the next byte is not ASCII
		kbd_extended = true;
		return 0;
	}

	if ((scancode & 0x80) != 0) { // release
		uint8_t code = scancode & 0x7Fu;

		if (code == 0x2A || code == 0x36) {
			kbd_shift = false;
		}
		kbd_extended = false;
		return 0;
	}

	switch (scancode) {
	case 0x2A:
	case 0x36:
		kbd_shift = true;
		return 0;
	case 0x3A:
		kbd_caps = !kbd_caps;
		return 0;
	case 0x1D: // Ctrl
	case 0x38: // Alt
		return 0;
	default:
		break;
	}

	if (kbd_extended) {
		kbd_extended = false;
		return 0;
	}

	char c = 0;

	if (kbd_shift && kbd_map_shift[scancode] != 0) {
		c = kbd_map_shift[scancode];
	} else {
		c = kbd_map[scancode];
		if (c >= 'a' && c <= 'z') {
			// Caps lock flips letters, and shift flips them back.
			bool upper = kbd_shift;

			if (kbd_caps) {
				upper = !upper;
			}
			if (upper) {
				c = (char)(c - 'a' + 'A');
			}
		}
	}
	return c;
}

bool kbd_getchar(char *out) {
	uint64_t oldest;
	uint8_t scancode;

	/*
	 * The IRQ1 handler is the only producer; this side only drains its queue.
	 * Reading the controller directly here would race the handler for the byte.
	 */
	oldest = ringbuf_oldest(&kbd_ring);

	if (kbd_read_pos < oldest) {
		kbd_read_pos = oldest; // the writer overwrote us: skip the lost bytes
	}

	while (ringbuf_peek(&kbd_ring, kbd_read_pos, &scancode, 1) == 1) {
		char c;

		kbd_read_pos++;
		c = kbd_decode(scancode);
		if (c != 0) {
			if (!kbd_reported) {
				kbd_reported = true;
				printk("kbd: first key arrived on IRQ%u", (unsigned)KBD_IRQ);
			}
			*out = c;
			return true;
		}
	}
	return false;
}

uint32_t kbd_pending(void) {
	uint64_t newest = ringbuf_newest(&kbd_ring);

	return newest > kbd_read_pos ? (uint32_t)(newest - kbd_read_pos) : 0;
}

bool kbd_ready(void) {
	return kbd_live;
}

void kbd_init(void) {
	uint8_t config;

	ringbuf_init(&kbd_ring, kbd_storage, sizeof(kbd_storage));
	kbd_read_pos = 0;
	kbd_shift = false;
	kbd_caps = false;
	kbd_extended = false;
	kbd_live = false;
	kbd_irqs = 0;
	kbd_reported = false;

	if (!kbd_write_cmd(0xAE)) { // enable the first PS/2 port
		printk("kbd: no PS/2 controller answered, keyboard disabled");
		return;
	}
	kbd_drain(16);
	if (!kbd_write_cmd(0x20) || !kbd_read_data(&config)) { // read the config byte
		printk("kbd: cannot read the controller config byte, keyboard disabled");
		return;
	}

	uint8_t before = config;

	config |= 0x01u;				// bit 0: enable the first port's interrupt (IRQ1)
	config &= (uint8_t)~0x10u; // bit 4: clear "disable keyboard" so the port runs
	config |= 0x40u;				// bit 6: request scancode translation (set 1)

	if (!kbd_write_cmd(0x60) || !kbd_write_data(config)) {
		printk("kbd: cannot write the controller config byte, keyboard disabled");
		return;
	}
	kbd_drain(16);

	// Read it back: if IRQ1 is still disabled the controller will never raise a
	// line and the I/O APIC entry above is irrelevant.
	uint8_t verify = 0;

	if (kbd_write_cmd(0x20) && kbd_read_data(&verify)) {
		printk("kbd: controller config 0x%02x -> 0x%02x (irq1 %s, translation %s)",
				 (unsigned)before,
				 (unsigned)verify,
				 (verify & 0x01) ? "enabled" : "DISABLED",
				 (verify & 0x40) ? "to set 1" : "raw set 2");
	}

	// Drop anything the firmware left in the output buffer.
	for (uint32_t i = 0; i < 16 && (inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) != 0; i++) {
		(void)inb(KBD_DATA_PORT);
	}

	/*
	 * Tell the keyboard itself to start scanning (0xF4).  This is a command for
	 * the *keyboard*, so it is written straight to the data port: prefixing it
	 * with the 0x60 controller command would mean "write the config byte" and
	 * 0xF4 would silently become the new configuration instead - with IRQ1
	 * cleared and scanning still off, which is a machine that ACKs everything
	 * and never produces a scancode.
	 *
	 * QEMU's PS/2 keyboard drops key events while scanning is disabled, so this
	 * is the difference between a working keyboard and a silent one.
	 */
	if (kbd_write_data(0xF4)) {
		uint8_t ack = 0;

		if (kbd_read_data(&ack) && ack != 0xFA) {
			printk("kbd: enable-scanning answered 0x%02x, not an ACK", (unsigned)ack);
		}
	}

	// Handler first, then unmask: unmasking a line that already has a request
	// pending delivers an interrupt immediately, and it lands in the
	// "unexpected vector" path if nothing is registered yet.
	irq_register(KBD_VECTOR, kbd_isr, NULL);

	if (!apic_route_isa_irq(KBD_IRQ, KBD_VECTOR)) {
		printk("kbd: IRQ %u could not be routed through the I/O APIC", (unsigned)KBD_IRQ);
	}

	/*
	 * Exactly one controller owns the line.  With an I/O APIC the redirection
	 * entry above is the path; the 8259 is only unmasked when there is no I/O
	 * APIC, where the PIC's INT reaches the CPU as an ExtINT through LINT0.
	 */
	if (!apic_ready()) {
		pic_set_mask(KBD_IRQ, false);
	}
	kbd_live = true;
	printk("kbd: PS/2 keyboard ready on vector %u", (unsigned)KBD_VECTOR);
}
