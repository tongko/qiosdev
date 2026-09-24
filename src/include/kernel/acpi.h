#pragma once

/*
 * ACPI table access.  The firmware leaves a pile of descriptive tables in RAM
 * and one pointer to them, the RSDP.  The bootloader copies that pointer into
 * bootinfo before ExitBootServices; everything else here is plain memory reads
 * through the HHDM, so the tables need no MMIO mapping of their own.
 *
 * No AML: DSDT/SSDT bytecode is deliberately out of scope.  The tables used
 * here (MADT, FADT, HPET, MCFG) are fixed-layout C structs and cover bring-up.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ACPI_MAX_IOAPICS 4
#define ACPI_MAX_ISOS 16

// Every ACPI table starts with this 36-byte header; length counts the header.
typedef struct __attribute__((packed)) {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} acpi_sdt_t;

/*
 * Root System Description Pointer.  Revision 0 is the 20-byte ACPI 1.0 layout;
 * revision >= 2 extends it with length, xsdt_address and extended_checksum.
 * The two checksums cover different spans, so both have to be checked.
 */
typedef struct __attribute__((packed)) {
	char signature[8]; // "RSD PTR " - not NUL terminated
	uint8_t checksum;	 // over the first 20 bytes
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
	uint32_t length;
	uint64_t xsdt_address;
	uint8_t extended_checksum; // over length bytes
	uint8_t reserved[3];
} acpi_rsdp_t;

// MADT entry types, the ones that matter for bring-up.
enum {
	ACPI_MADT_TYPE_LAPIC = 0,
	ACPI_MADT_TYPE_IOAPIC = 1,
	ACPI_MADT_TYPE_ISO = 2,
	ACPI_MADT_TYPE_LAPIC_NMI = 4,
	ACPI_MADT_TYPE_LAPIC_OVERRIDE = 5,
	ACPI_MADT_TYPE_X2APIC = 9,
};

// Interrupt Source Override flags: bits 0-1 polarity, bits 2-3 trigger.
#define ACPI_MPS_INTI_ACTIVE_HIGH 0x1
#define ACPI_MPS_INTI_ACTIVE_LOW 0x3
#define ACPI_MPS_INTI_EDGE 0x4
#define ACPI_MPS_INTI_LEVEL 0xC

// One I/O APIC, as the MADT describes it.
typedef struct {
	uint8_t id;
	uint32_t pa;			 // MMIO base, page aligned
	uint32_t gsi_base; // first GSI this APIC handles
} acpi_ioapic_t;

/*
 * What the MADT told us.  The next step (mask every I/O APIC redirection
 * entry, then unmask the keyboard line through it and EOI via the LAPIC MMIO
 * window) builds directly on this.
 *
 * iso_gsi holds the resolved ISA IRQ -> GSI map: identity where the firmware
 * gave no override, so IRQ 0 -> GSI 0 unless an override says otherwise (a PC
 * almost always maps IRQ 0 to GSI 2).  iso_flags is the raw override flags, or
 * 0 when there was no override for that IRQ.
 */
typedef struct {
	bool present;
	uint64_t lapic_pa; // 32-bit field in the MADT header, a type-5 entry wins
	uint32_t cpu_count; // local APICs with the enabled flag set
	acpi_ioapic_t ioapics[ACPI_MAX_IOAPICS];
	uint32_t ioapic_count;
	uint32_t iso_gsi[ACPI_MAX_ISOS];
	uint16_t iso_flags[ACPI_MAX_ISOS];
	uint32_t iso_count;
} acpi_madt_t;

// Validate the RSDP and XSDT/RSDT, dump every table, parse the MADT.
void acpi_init(void);

// Walk the XSDT/RSDT again and return the first table with this signature
// (must be a 4-character array, not a C string).  NULL when it is absent or
// fails its checksum.
const acpi_sdt_t *acpi_find(const char signature[4]);

// The parsed MADT, or NULL when there was none.
const acpi_madt_t *acpi_madt(void);

// The GSI an ISA IRQ is wired to, after the overrides.
uint32_t acpi_gsi_for_isa_irq(uint32_t irq);
