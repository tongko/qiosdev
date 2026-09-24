// acpi.c - RSDP validation, the XSDT/RSDT walk, and the MADT parse.
//
// The tables live in ordinary RAM: the bootloader stashes the RSDP physical
// address in bootinfo and everything here reads it through the HHDM.  Only the
// controllers the MADT points at (the LAPIC and I/O APIC MMIO windows) need a
// mapping of their own, with the cache disabled, and that is the next step.
#include <kernel/acpi.h>
#include <kernel/buddy.h>
#include <kernel/global.h>
#include <kernel/klog.h>
#include <libk/string.h>

static const acpi_sdt_t *g_root; // whichever of XSDT/RSDT we ended up using
static bool g_root_is_xsdt;
static const acpi_sdt_t *g_madt_sdt;
static acpi_madt_t g_madt;

// --- MADT entry layouts -----------------------------------------------------
// Every entry starts with a type and a total length and is walked by that
// length: they are not fixed size, and a field may only be read once its own
// smaller length has been checked.

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
} acpi_madt_entry_t;

typedef struct __attribute__((packed)) {
	acpi_sdt_t header;
	uint32_t lapic_address;
	uint32_t flags;
} acpi_madt_header_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint8_t processor_id;
	uint8_t apic_id;
	uint32_t flags; // bit 0: enabled
} acpi_madt_lapic_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint8_t ioapic_id;
	uint8_t reserved;
	uint32_t address;
	uint32_t gsi_base;
} acpi_madt_ioapic_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint8_t bus;
	uint8_t source; // the ISA IRQ
	uint32_t gsi;
	uint16_t flags;
} acpi_madt_iso_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint8_t processor_id;
	uint16_t flags;
	uint8_t lint; // 0 = LINT0, 1 = LINT1
} acpi_madt_lapic_nmi_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint16_t reserved;
	uint64_t address; // the 64-bit LAPIC base
} acpi_madt_lapic_override_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint16_t reserved;
	uint32_t x2apic_id;
	uint32_t flags;
	uint32_t processor_id;
} acpi_madt_x2apic_t;

// --- helpers ----------------------------------------------------------------

// A table is valid when its bytes sum to zero (mod 256).  Skipping this is how
// people end up parsing garbage and blaming the firmware.
static bool acpi_checksum_ok(const void *data, size_t length) {
	const uint8_t *bytes = data;
	uint8_t sum = 0;

	for (size_t i = 0; i < length; i++) {
		sum += bytes[i];
	}
	return sum == 0;
}

// Signatures and OEM ids are fixed-size fields, not C strings, so copy them
// into a NUL-terminated buffer before handing them to printk.
static void field_to_str(const char *field, size_t length, char *out) {
	memcpy(out, field, length);
	out[length] = '\0';
}

static uint32_t acpi_root_entry_count(void) {
	return (g_root->length - (uint32_t)sizeof(acpi_sdt_t)) / (g_root_is_xsdt ? 8u : 4u);
}

static const acpi_sdt_t *acpi_root_entry(uint32_t index) {
	const uint8_t *entries = (const uint8_t *)g_root + sizeof(acpi_sdt_t);
	uint64_t pa = g_root_is_xsdt ? ((const uint64_t *)entries)[index] : ((const uint32_t *)entries)[index];

	return HHDM(pa);
}

static const char *polarity_name(uint16_t flags) {
	switch (flags & 0x3) {
	case ACPI_MPS_INTI_ACTIVE_HIGH:
		return "active high";
	case ACPI_MPS_INTI_ACTIVE_LOW:
		return "active low";
	default:
		return "bus polarity";
	}
}

static const char *trigger_name(uint16_t flags) {
	switch (flags & 0xC) {
	case ACPI_MPS_INTI_EDGE:
		return "edge";
	case ACPI_MPS_INTI_LEVEL:
		return "level";
	default:
		return "bus trigger";
	}
}

// --- MADT -------------------------------------------------------------------

static void acpi_parse_madt(const acpi_sdt_t *madt) {
	const acpi_madt_header_t *header = (const acpi_madt_header_t *)madt;
	const uint8_t *entry = (const uint8_t *)madt + sizeof(acpi_madt_header_t);
	const uint8_t *end = (const uint8_t *)madt + madt->length;

	memset(&g_madt, 0, sizeof(g_madt));
	g_madt.present = true;
	g_madt.lapic_pa = header->lapic_address;

	// Identity until an override says otherwise: without this IRQ 0 would look
	// like GSI 0, but a PC wires it to GSI 2.
	for (uint32_t irq = 0; irq < ACPI_MAX_ISOS; irq++) {
		g_madt.iso_gsi[irq] = irq;
	}

	while (entry + sizeof(acpi_madt_entry_t) <= end) {
		const acpi_madt_entry_t *e = (const acpi_madt_entry_t *)entry;

		if (e->length < sizeof(acpi_madt_entry_t) || entry + e->length > end) {
			printk("acpi: madt: entry type %u has a bad length (%u), stopping", (unsigned)e->type, (unsigned)e->length);
			break;
		}

		switch (e->type) {
		case ACPI_MADT_TYPE_LAPIC: {
			const acpi_madt_lapic_t *lapic = (const acpi_madt_lapic_t *)e;

			if (lapic->length >= sizeof(*lapic)) {
				bool enabled = (lapic->flags & 1u) != 0;

				if (enabled) {
					g_madt.cpu_count++;
				}
				printk("acpi:   cpu %u apic id %u %s",
							 (unsigned)lapic->processor_id,
							 (unsigned)lapic->apic_id,
							 enabled ? "enabled" : "disabled");
			}
			break;
		}
		case ACPI_MADT_TYPE_IOAPIC: {
			const acpi_madt_ioapic_t *io = (const acpi_madt_ioapic_t *)e;

			if (io->length >= sizeof(*io) && g_madt.ioapic_count < ACPI_MAX_IOAPICS) {
				acpi_ioapic_t *out = &g_madt.ioapics[g_madt.ioapic_count++];

				out->id = io->ioapic_id;
				out->pa = io->address;
				out->gsi_base = io->gsi_base;
				printk("acpi:   ioapic %u at 0x%x, gsi base %u",
							 (unsigned)io->ioapic_id,
							 (unsigned)io->address,
							 (unsigned)io->gsi_base);
			}
			break;
		}
		case ACPI_MADT_TYPE_ISO: {
			const acpi_madt_iso_t *iso = (const acpi_madt_iso_t *)e;

			if (iso->length >= sizeof(*iso)) {
				printk("acpi:   irq %u -> gsi %u (%s, %s)",
							 (unsigned)iso->source,
							 (unsigned)iso->gsi,
							 polarity_name(iso->flags),
							 trigger_name(iso->flags));
				if (iso->source < ACPI_MAX_ISOS) {
					g_madt.iso_gsi[iso->source] = iso->gsi;
					g_madt.iso_flags[iso->source] = iso->flags;
				}
				g_madt.iso_count++;
			}
			break;
		}
		case ACPI_MADT_TYPE_LAPIC_NMI: {
			const acpi_madt_lapic_nmi_t *nmi = (const acpi_madt_lapic_nmi_t *)e;

			if (nmi->length >= sizeof(*nmi)) {
				printk("acpi:   lapic nmi, processor %u -> LINT%u", (unsigned)nmi->processor_id, (unsigned)nmi->lint);
			}
			break;
		}
		case ACPI_MADT_TYPE_LAPIC_OVERRIDE: {
			const acpi_madt_lapic_override_t *ov = (const acpi_madt_lapic_override_t *)e;

			if (ov->length >= sizeof(*ov)) {
				g_madt.lapic_pa = ov->address;
				printk("acpi:   lapic base overridden to 0x%llx", (unsigned long long)ov->address);
			}
			break;
		}
		case ACPI_MADT_TYPE_X2APIC: {
			const acpi_madt_x2apic_t *x2 = (const acpi_madt_x2apic_t *)e;

			if (x2->length >= sizeof(*x2)) {
				bool enabled = (x2->flags & 1u) != 0;

				if (enabled) {
					g_madt.cpu_count++;
				}
				printk("acpi:   x2apic id %u %s", (unsigned)x2->x2apic_id, enabled ? "enabled" : "disabled");
			}
			break;
		}
		default:
			printk("acpi:   madt entry type %u (%u bytes, ignored)", (unsigned)e->type, (unsigned)e->length);
			break;
		}

		entry += e->length;
	}

	printk("acpi: madt: %u cpu(s), %u ioapic(s), %u override(s), lapic at 0x%llx",
				 (unsigned)g_madt.cpu_count,
				 (unsigned)g_madt.ioapic_count,
				 (unsigned)g_madt.iso_count,
				 (unsigned long long)g_madt.lapic_pa);
}

// --- public -----------------------------------------------------------------

const acpi_sdt_t *acpi_find(const char signature[4]) {
	if (g_root == NULL) {
		return NULL;
	}

	for (uint32_t i = 0; i < acpi_root_entry_count(); i++) {
		const acpi_sdt_t *table = acpi_root_entry(i);

		if (memcmp(table->signature, signature, 4) == 0 && table->length >= sizeof(acpi_sdt_t) &&
				acpi_checksum_ok(table, table->length)) {
			return table;
		}
	}
	return NULL;
}

const acpi_madt_t *acpi_madt(void) {
	return g_madt.present ? &g_madt : NULL;
}

uint32_t acpi_gsi_for_isa_irq(uint32_t irq) {
	if (irq >= ACPI_MAX_ISOS || !g_madt.present) {
		return irq;
	}
	return g_madt.iso_gsi[irq];
}

void acpi_init(void) {
	char text[16];

	if (_bi.acpi_rsdp_pa == 0) {
		printk("acpi: the bootloader handed over no RSDP");
		return;
	}

	const acpi_rsdp_t *rsdp = HHDM(_bi.acpi_rsdp_pa);

	if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
		printk("acpi: bad RSDP signature at 0x%llx", (unsigned long long)_bi.acpi_rsdp_pa);
		return;
	}
	if (!acpi_checksum_ok(rsdp, 20)) {
		printk("acpi: RSDP checksum failed");
		return;
	}

	field_to_str(rsdp->oem_id, 6, text);
	printk("acpi: RSDP rev %u, oem \"%s\"", (unsigned)rsdp->revision, text);

	// Prefer the XSDT: with 4 GiB or more of RAM the firmware can place tables
	// above 4 GiB, where the RSDT's 32-bit pointers truncate.
	if (rsdp->revision >= 2 && rsdp->length >= 36) {
		if (acpi_checksum_ok(rsdp, rsdp->length) && rsdp->xsdt_address != 0) {
			g_root = HHDM(rsdp->xsdt_address);
			g_root_is_xsdt = true;
		} else {
			printk("acpi: RSDP extended checksum failed, falling back to the RSDT");
		}
	}
	if (g_root == NULL) {
		g_root = HHDM((uint64_t)rsdp->rsdt_address);
		g_root_is_xsdt = false;
	}

	field_to_str(g_root->signature, 4, text);
	if (memcmp(g_root->signature, g_root_is_xsdt ? "XSDT" : "RSDT", 4) != 0 || g_root->length < sizeof(acpi_sdt_t) ||
			!acpi_checksum_ok(g_root, g_root->length)) {
		printk("acpi: %s at 0x%llx is unusable (signature \"%s\", %u bytes, checksum failed)",
					 g_root_is_xsdt ? "XSDT" : "RSDT",
					 (unsigned long long)(g_root_is_xsdt ? rsdp->xsdt_address : rsdp->rsdt_address),
					 text,
					 (unsigned)g_root->length);
		g_root = NULL;
		return;
	}

	uint32_t count = acpi_root_entry_count();
	uint32_t good = 0;

	printk("acpi: %s has %u table(s)", text, (unsigned)count);

	for (uint32_t i = 0; i < count; i++) {
		const acpi_sdt_t *table = acpi_root_entry(i);
		char signature[5];
		char oem_table[9];
		bool ok;

		if (table->length < sizeof(acpi_sdt_t)) {
			printk("acpi:   entry %u has length %u, skipping", (unsigned)i, (unsigned)table->length);
			continue;
		}

		field_to_str(table->signature, 4, signature);
		field_to_str(table->oem_table_id, 8, oem_table);
		ok = acpi_checksum_ok(table, table->length);
		printk("acpi:   %s rev %u, %u bytes, \"%s\", checksum %s",
					 signature,
					 (unsigned)table->revision,
					 (unsigned)table->length,
					 oem_table,
					 ok ? "ok" : "BAD");

		if (!ok) {
			continue;
		}
		good++;
		if (memcmp(table->signature, "APIC", 4) == 0 && g_madt_sdt == NULL) {
			g_madt_sdt = table;
		}
	}

	printk("acpi: %u of %u table(s) passed their checksum", (unsigned)good, (unsigned)count);

	if (g_madt_sdt != NULL) {
		acpi_parse_madt(g_madt_sdt);
	} else {
		printk("acpi: no MADT, interrupting will have to stay as it is");
	}
}
