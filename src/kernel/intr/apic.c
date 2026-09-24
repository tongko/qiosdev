// apic.c - local APIC and I/O APIC bring-up over MMIO.
//
// Everything is masked to start with: an interrupt on a vector nobody handles
// is what used to turn into a #GP in the middle of the idle loop.  Drivers
// unmask exactly the line they own, through apic_route_isa_irq().
#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/buddy.h>
#include <kernel/global.h>
#include <kernel/idt.h>
#include <kernel/io.h>
#include <kernel/klog.h>
#include <kernel/mm.h>
#include <kernel/tsc.h>

// Local APIC registers, as offsets from its MMIO base.
#define LAPIC_ID 0x020
#define LAPIC_VERSION 0x030
#define LAPIC_TPR 0x080
#define LAPIC_EOI 0x0B0
#define LAPIC_SVR 0x0F0
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_TIMER_INIT 0x380
#define LAPIC_TIMER_CURRENT 0x390
#define LAPIC_TIMER_DIVIDE 0x3E0
#define LAPIC_TIMER_PERIODIC (1u << 17)
#define LAPIC_TIMER_DIVIDE_BY_16 0x3
#define LAPIC_TIMER_MAX 0xFFFFFFFFu

#define MSR_IA32_APIC_BASE 0x1Bu
#define APIC_BASE_ENABLE (1ull << 11)

static inline uint64_t rdmsr(uint32_t msr) {
	uint32_t lo, hi;

	__asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
	__asm__ volatile("wrmsr" ::"a"((uint32_t)value), "d"((uint32_t)(value >> 32)), "c"(msr));
}

#define LAPIC_SVR_ENABLE (1u << 8)
#define LAPIC_LVT_MASKED (1u << 16)
#define LAPIC_LVT_DELIVERY_EXTINT (0x7u << 8)
#define LAPIC_SPURIOUS_VECTOR 0xFFu

// I/O APIC registers, reached indirectly through IOREGSEL / IOWIN.
#define IOAPIC_IOREGSEL 0x00
#define IOAPIC_IOWIN 0x10
#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VERSION 0x01
#define IOAPIC_REG_REDIRECT 0x10

// Redirection entry bits.  Entries are 64 bit: the low dword carries the
// vector and the polarity/trigger/mask bits, the high dword the destination.
#define IOAPIC_RTE_ACTIVE_LOW (1u << 13)
#define IOAPIC_RTE_LEVEL (1u << 15)
#define IOAPIC_RTE_MASKED (1u << 16)

static volatile uint32_t *g_lapic;
static volatile uint32_t *g_ioapic;
static uint64_t g_lapic_pa;
static uint32_t g_ioapic_pa;
static uint32_t g_ioapic_gsi_base;
static uint32_t g_ioapic_lines;
static uint32_t g_apic_id;
static bool g_ready;

static void lapic_write(uint32_t reg, uint32_t value) {
	g_lapic[reg / 4] = value;
}

static uint32_t lapic_read(uint32_t reg) {
	return g_lapic[reg / 4];
}

static void ioapic_write(uint32_t reg, uint32_t value) {
	g_ioapic[IOAPIC_IOREGSEL / 4] = reg;
	__asm__ volatile("" ::: "memory");
	g_ioapic[IOAPIC_IOWIN / 4] = value;
	__asm__ volatile("" ::: "memory");
}

static uint32_t ioapic_read(uint32_t reg) {
	g_ioapic[IOAPIC_IOREGSEL / 4] = reg;
	__asm__ volatile("" ::: "memory");
	return g_ioapic[IOAPIC_IOWIN / 4];
}

static void tlb_flush_page(void *va) {
	__asm__ volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/*
 * MMIO must be uncached, and the HHDM already covers these addresses with a
 * normal mapping, so re-map them and drop the stale TLB entries.  Both APIC
 * windows are 2 MiB aligned on every machine I have seen, which lets the
 * mapping use a single huge page.
 */
static void *map_mmio(paddr_t pa, size_t size) {
	vaddr_t va = (vaddr_t)HHDM(pa);
	uint64_t *pml4 = HHDM(_bi.pml4_paddr);
	bool huge = (pa % PAGE_HUGE_SIZE == 0) && ((vaddr_t)HHDM(pa) % PAGE_HUGE_SIZE == 0);
	size_t map_size = huge ? PAGE_HUGE_SIZE : ((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

	if (!mm_map_virt_addr(pml4, va, pa, PAGE_PCD | PAGE_PWT, map_size, huge ? PAGE_2M : PAGE_4K)) {
		return NULL;
	}

	for (size_t off = 0; off < map_size; off += PAGE_SIZE) {
		tlb_flush_page((void *)(va + off));
	}
	return (void *)va;
}

static bool gsi_to_line(uint32_t gsi, uint32_t *line) {
	if (g_ioapic == NULL || gsi < g_ioapic_gsi_base) {
		return false;
	}
	*line = gsi - g_ioapic_gsi_base;
	return *line < g_ioapic_lines;
}

void apic_mask_gsi(uint32_t gsi) {
	uint32_t line;

	if (!gsi_to_line(gsi, &line)) {
		return;
	}

	ioapic_write(IOAPIC_REG_REDIRECT + line * 2 + 1, 0); // destination
	ioapic_write(IOAPIC_REG_REDIRECT + line * 2, IOAPIC_RTE_MASKED);
}

bool apic_route_gsi(uint32_t gsi, uint8_t vector, uint16_t mps_flags) {
	uint32_t line;
	uint32_t low;

	if (!gsi_to_line(gsi, &line)) {
		printk("apic: gsi %u is not on the I/O APIC", (unsigned)gsi);
		return false;
	}

	low = vector;
	if ((mps_flags & 0x3) == ACPI_MPS_INTI_ACTIVE_LOW) {
		low |= IOAPIC_RTE_ACTIVE_LOW;
	}
	if ((mps_flags & 0xC) == ACPI_MPS_INTI_LEVEL) {
		low |= IOAPIC_RTE_LEVEL;
	}

	// Destination is the low byte of the high dword: this CPU's local APIC.
	ioapic_write(IOAPIC_REG_REDIRECT + line * 2 + 1, g_apic_id << 24);
	ioapic_write(IOAPIC_REG_REDIRECT + line * 2, low);

	// Read it back: if the MMIO write went nowhere there will be no interrupts
	// at all, and one line here beats an hour of guessing.
	printk("apic:   gsi %u rte[%u] = 0x%08x%08x",
				 (unsigned)gsi,
				 (unsigned)line,
				 (unsigned)ioapic_read(IOAPIC_REG_REDIRECT + line * 2 + 1),
				 (unsigned)ioapic_read(IOAPIC_REG_REDIRECT + line * 2));
	return true;
}

bool apic_route_isa_irq(uint32_t irq, uint8_t vector) {
	const acpi_madt_t *madt = acpi_madt();
	uint16_t flags = 0;
	uint32_t gsi = acpi_gsi_for_isa_irq(irq);

	if (madt != NULL && irq < ACPI_MAX_ISOS) {
		flags = madt->iso_flags[irq];
	}

	if (!apic_route_gsi(gsi, vector, flags)) {
		return false;
	}

	printk("apic: irq %u -> gsi %u -> vector %u (%s, %s)",
				 (unsigned)irq,
				 (unsigned)gsi,
				 (unsigned)vector,
				 (flags & 0x3) == ACPI_MPS_INTI_ACTIVE_LOW ? "active low" : "active high",
				 (flags & 0xC) == ACPI_MPS_INTI_LEVEL ? "level" : "edge");
	return true;
}

static volatile uint64_t g_ticks;

static void apic_timer_isr(void *ctx, interrupt_frame_t *frame) {
	(void)ctx;
	(void)frame;
	g_ticks++;
}

uint64_t apic_timer_ticks(void) {
	return g_ticks;
}

bool apic_timer_start(uint32_t hz) {
	if (!g_ready || hz == 0) {
		return false;
	}

	// Measure the LAPIC timer against the TSC: start a one-shot with the largest
	// count, then see how far it got in 10 ms.
	lapic_write(LAPIC_TIMER_DIVIDE, LAPIC_TIMER_DIVIDE_BY_16);
	lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
	lapic_write(LAPIC_TIMER_INIT, LAPIC_TIMER_MAX);

	uint64_t window = _bi.tsc_hz / 100;
	uint64_t start = rdtsc();

	while (rdtsc() - start < window) {
		cpu_relax();
	}

	uint32_t elapsed = LAPIC_TIMER_MAX - lapic_read(LAPIC_TIMER_CURRENT);
	uint64_t lapic_hz = (uint64_t)elapsed * 100u;
	uint32_t count = (uint32_t)(lapic_hz / hz);

	if (elapsed == 0 || count == 0) {
		printk("apic: timer calibration failed (%u ticks in 10 ms)", (unsigned)elapsed);
		return false;
	}

	irq_register(APIC_TIMER_VECTOR, apic_timer_isr, NULL);
	lapic_write(LAPIC_TIMER_DIVIDE, LAPIC_TIMER_DIVIDE_BY_16);
	lapic_write(LAPIC_LVT_TIMER, APIC_TIMER_VECTOR | LAPIC_TIMER_PERIODIC);
	lapic_write(LAPIC_TIMER_INIT, count);

	printk("apic: tick %u Hz on vector %u (lapic ~%llu Hz / 16, count %u)",
				 (unsigned)hz,
				 (unsigned)APIC_TIMER_VECTOR,
				 (unsigned long long)lapic_hz,
				 (unsigned)count);
	return true;
}

bool apic_ready(void) {
	return g_ready;
}

void apic_eoi(void) {
	if (g_lapic != NULL) {
		lapic_write(LAPIC_EOI, 0);
	}
}

uint64_t apic_lapic_base(void) {
	return g_lapic_pa;
}

uint32_t apic_ioapic_base(void) {
	return g_ioapic_pa;
}

uint32_t apic_ioapic_lines(void) {
	return g_ioapic_lines;
}

uint32_t apic_cpu_apic_id(void) {
	return g_apic_id;
}

bool apic_init(void) {
	const acpi_madt_t *madt = acpi_madt();

	if (madt == NULL || madt->lapic_pa == 0) {
		printk("apic: no MADT, the 8259 stays in charge");
		return false;
	}
	if (madt->ioapic_count == 0) {
		printk("apic: MADT describes no I/O APIC, the 8259 stays in charge");
		return false;
	}

	g_lapic_pa = madt->lapic_pa;
	g_lapic = map_mmio((paddr_t)g_lapic_pa, PAGE_SIZE);
	if (g_lapic == NULL) {
		printk("apic: cannot map the local APIC at 0x%llx", (unsigned long long)g_lapic_pa);
		return false;
	}

	g_ioapic_pa = madt->ioapics[0].pa;
	g_ioapic_gsi_base = madt->ioapics[0].gsi_base;
	g_ioapic = map_mmio((paddr_t)g_ioapic_pa, PAGE_SIZE);
	if (g_ioapic == NULL) {
		printk("apic: cannot map the I/O APIC at 0x%x", (unsigned)g_ioapic_pa);
		return false;
	}

	// The local APIC also has to be enabled globally: IA32_APIC_BASE bit 11.
	// SVR bit 8 alone is not enough and interrupts are dropped silently.  0x1B
	// is a core MSR, safe to write - unlike the x2APIC register window, which is
	// what faulted when this was first attempted from idt_init().
	uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);

	if ((apic_base & APIC_BASE_ENABLE) == 0) {
		wrmsr(MSR_IA32_APIC_BASE, apic_base | APIC_BASE_ENABLE);
		apic_base = rdmsr(MSR_IA32_APIC_BASE);
	}
	printk("apic: IA32_APIC_BASE 0x%llx (%s), mmio base 0x%llx",
				 (unsigned long long)apic_base,
				 (apic_base & APIC_BASE_ENABLE) ? "enabled" : "DISABLED",
				 (unsigned long long)(apic_base & 0xFFFFF000ull));

	// Local APIC: software enable (interrupts are silently dropped without
	// SVR bit 8), accept every priority, mask the lines we do not handle yet.
	lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);
	lapic_write(LAPIC_TPR, 0);
	lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
	/*
	 * LINT0 is where the 8259's INT line lands on a modern x86: the legacy PIC
	 * is delivered as an ExtINT through the local APIC (virtual wire mode).
	 * Masking it - as this did - leaves the PIC's requests latched and unmasked
	 * in its IRR, acknowledged by nobody, which is a keyboard that raises a line
	 * into the void.  LINT1 stays masked; the MADT puts an NMI there.
	 */
	lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_DELIVERY_EXTINT);
	lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
	lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);

	g_apic_id = lapic_read(LAPIC_ID) >> 24;
	g_ioapic_lines = ((ioapic_read(IOAPIC_REG_VERSION) >> 16) & 0xFFu) + 1;

	for (uint32_t line = 0; line < g_ioapic_lines; line++) {
		apic_mask_gsi(g_ioapic_gsi_base + line);
	}

	g_ready = true;
	printk("apic: lapic 0x%llx (id %u, version 0x%x), ioapic 0x%x (id %u, %u lines, gsi base %u)",
				 (unsigned long long)g_lapic_pa,
				 (unsigned)g_apic_id,
				 (unsigned)lapic_read(LAPIC_VERSION) & 0xFFu,
				 (unsigned)g_ioapic_pa,
				 (unsigned)(ioapic_read(IOAPIC_REG_ID) >> 24),
				 (unsigned)g_ioapic_lines,
				 (unsigned)g_ioapic_gsi_base);
	printk("apic: %u I/O APIC line(s) masked, local APIC enabled",
				 (unsigned)g_ioapic_lines);
	return true;
}
