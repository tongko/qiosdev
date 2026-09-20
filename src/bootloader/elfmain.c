#include <bootinfo.h>
#include <efi.h>
#include <efiglobal.h>
#include <efilib.h>
#include <efimain.h>
#include <elf64.h>
#include <extfs.h>
#include <memmap.h>
#include <paging.h>

#define COLOR_ARGB(a, r, g, b) (((UINT32)(a) << 24) | ((UINT32)(r) << 16) | ((UINT32)(g) << 8) | (UINT32)(b))

#define IA32_EFER_MSR 0xC0000080
#define IA32_EFER_NXE (1ULL << 11)
#define READ_EFER_MSR(high, low) __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(IA32_EFER_MSR))

// should be in CPU module, but for now...
static void ensure_nxe_enabled(void) {
	UINT32 low, high;

	// Read the 64-bit MSR value using RDMSR
	// RDMSR reads the register specified in ECX and splits it into EDX:EAX
	READ_EFER_MSR(high, low);

	// Combine high and low 32-bit values into a single 64-bit integer
	UINT64 efer = ((UINT64)high << 32) | low;

	// Check if the NXE bit (Bit 11) is set
	if (efer & IA32_EFER_NXE) {
		Print(u"[ensure_nxe_enabled] IA32_EFER.NXE is ALREADY enabled by UEFI "
				u"firmware.\n");
		_nxe_enabled = true;
	} else {
		Print(u"[ensure_nxe_enabled] IA32_EFER.NXE is DISABLED. Turning it on "
				u"now... ");

		// Set Bit 11
		efer |= IA32_EFER_NXE;
		low = (UINT32)(efer & 0xFFFFFFFF);
		high = (UINT32)(efer >> 32);

		// Write it back to the CPU using WRMSR
		__asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(IA32_EFER_MSR));

		// Test if CPU support it
		READ_EFER_MSR(high, low);
		efer = ((UINT64)high << 32) | low;
		_nxe_enabled = (efer & IA32_EFER_NXE);
		if (_nxe_enabled) {
			Print(u"IA32_EFER.NXE set.\r\n");
		} else {
			Print(u"CPU doesn't support IA32_EFER.NXE\r\n");
		}
	}
}

static inline UINTN rdtsc(void) {
	UINT32 lo, hi;

	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((UINTN)hi << 32) | lo;
}

static void tsc_init(bootinfo_t *bi) {
	UINTN tsc_start = rdtsc();
	BS->Stall(100000); // Stall for 100 ms
	UINTN tsc_end = rdtsc();
	bi->tsc_freq_hz = (tsc_end - tsc_start) * 10;
	bi->tsc_start = 0; // We start from kernel, not here
}

EFI_STATUS efi_main(EFI_HANDLE ih, EFI_SYSTEM_TABLE *st) {
	// What this boot loader does:
	// 1. Setup simple paging, but not switching to it yet.
	// 2. Load kernel from Ext4FS volumn.
	// 3. Setup boot info struct, including framebuffer and memory map.
	// 4. Critical ExitBootService retry loop
	// 5. Switch to simple paging, jump to kernel entry point.

	// ====================================================
	// 0. Initialize GNU-EFI lib
	// ====================================================
	bootinfo_t bi = {0};
	// _ih = ih;
	// _st = st;
	InitializeLib(ih, st);
	_himage = ih;	// set as global variable
	tsc_init(&bi); // Init timing

	// Check if CPU support IA32_EFER.NXE
	ensure_nxe_enabled();

	Print(u"=== QiOS EFI Boot Loader 0.0.1 ===\r\n");
	EFI_STATUS status = mem_init(&bi);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [efi_main] Init memory failed: %r%N", status);
		return status;
	}

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_MEM)
	Print(u"[DEBUG][efi_main] Boot info (memmap):\r\n");
	Print(u"[DEBUG][efi_main] Memory map base : 0x%lx\r\n", bi.mem_map.map);
	Print(u"[DEBUG][efi_main] Map size        : %lu\r\n", bi.mem_map.map_size);
	Print(u"[DEBUG][efi_main] Descriptor size : %lu\r\n", bi.mem_map.desc_size);
#endif

	// ====================================================
	// 1. Setup simple paging, but not switching to it yet.
	// ====================================================
	status = paging_init(&bi);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [efi_main] Init memory failed: %r%N", status);
		return status;
	}

	// ====================================================
	// 2. Load kernel
	// ====================================================
	// Load and start ExtFS driver
	status = extfs_init(u"\\EFI\\BOOT\\DRIVERS\\ext4_x64.efi");
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [efi_main] Init ExtFS failed: %r%N", status);
		return status;
	}

	EFI_FILE_HANDLE hfile = NULL;
	status = open_file(u"\\sys\\qios.elf", &hfile);
	if (EFI_ERROR(status)) {
		if (hfile) {
			hfile->Close(hfile);
		}
		Print(u"%E❌ [efi_main] Kernel not found: %r%N\r\n", status);
		return status;
	}

	EFI_VIRTUAL_ADDRESS kbuf = 0;
	status = load_elf(hfile, &kbuf);
	hfile->Close(hfile);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [efi_main] Failed to load kernel: %r%N\r\n", status);
		return status;
	}

	// ================================================================
	// 3. Setup boot info struct, including framebuffer and memory map.
	// ================================================================
	status = bootinfo_init(&bi);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [efi_main] Failed to init boot info: %r%N\r\n", status);
		return status;
	}

	// hhdm_init() only mapped RAM (0 .. highest_phys_addr).  The GOP
	// framebuffer is MMIO and can sit far above that (a 64-bit BAR on this
	// machine), so HHDM(fb) would be an unmapped address and the kernel's
	// first present() would page fault.  Map it, uncached, before the kernel
	// ever sees it.
	if (bi.frame_buff.base_addr != NULL && bi.frame_buff.size != 0) {
		EFI_PHYSICAL_ADDRESS fb_pa = (EFI_PHYSICAL_ADDRESS)(UINTN)bi.frame_buff.base_addr;

		// The firmware can hand back its own high-half alias of the BAR
		// (HHDM_OFFSET + pa): normalise it to a physical address first.
		if (fb_pa >= HHDM_OFFSET) {
			fb_pa -= HHDM_OFFSET;
		}

		// 4K pages, PCD|PWT = uncached.  Do not use PAGE_UNCACHEABLE here:
		// it has PAGE_HUGE set, and bit 7 of a 4K PTE means PAT, not huge.
		status = map_virt_addr(HHDM(fb_pa), fb_pa, PAGE_PCD | PAGE_PWT, bi.frame_buff.size, PAGE_4K);
		if (EFI_ERROR(status)) {
			Print(u"%E❌ [efi_main] Mapping the framebuffer failed: %r%N\r\n", status);
			return status;
		}

		// This is the address the kernel uses.
		bi.frame_buff.base_addr = (UINT32 *)(UINTN)HHDM(fb_pa);
		Print(u"[efi_main] Framebuffer pa 0x%lx -> HHDM 0x%lx (%lu bytes, uncached).\r\n", fb_pa,
					(UINTN)HHDM(fb_pa), bi.frame_buff.size);
	}

	EFI_PHYSICAL_ADDRESS stack_pa = alloc_pages(AllocateAnyPages, EfiLoaderData, STACK_PAGE_SIZE);
	Print(u"[efi_main] Stack physical address: 0x%lx\r\n", stack_pa);
	bi.kstack_base = stack_pa;

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_PAGING)
	Print(u"[DEBUG] FB paddr: 0x%lx\r\n", bi.frame_buff.base_addr);
#endif
	reorder_alloc(&bi);
#if defined(EFI_DEBUG) && defined(EFI_DEBUG_PAGING)
	Print(u"[DEBUG] Allocated pages:\r\n");
	for (UINTN i = 0; i < 20; i++) {
		Print(u"[DEBUG]\t%d: start=0x%lx, end=0x%lx\r\n", i, bi.alloc_pages[i].pstart, bi.alloc_pages[i].pend);
	}
#endif

	Print(u"[efi_main] Exiting boot service... ");
	do {
		Print(u" .");
		EFI_MEMORY_DESCRIPTOR *map;
		UINTN msz, dsz, key;
		BS->FreePool(bi.mem_map.map);
		status = get_memmap(&map, &msz, &dsz, &key);
		if (EFI_ERROR(status)) {
			Print(u"❌");
			return status;
		}
		bi.mem_map.map = map;
		bi.mem_map.map_size = msz;
		bi.mem_map.desc_size = dsz;
		status = BS->ExitBootServices(_himage, key);
		if (status != EFI_SUCCESS && status != EFI_INVALID_PARAMETER) {
			Print(u"%E❌ [efi_main] ExitBootServices failed: %r%N\r\n", status);
			return status;
		}
	} while (status == EFI_INVALID_PARAMETER);

	// The debug rectangle that used to be painted here is gone: the kernel now
	// paints the background through the framebuffer driver, using the HHDM
	// address mapped above.  Writing to the raw BAR address only worked while
	// the firmware's own page tables were still loaded.
	SET_CR3(bi.pml4_paddr);

	EFI_VIRTUAL_ADDRESS stack_top = HHDM(stack_pa) + (STACK_PAGE_SIZE * EFI_PAGE_SIZE);
	(void)stack_top;
	__asm__("mov %[stack], %%rsp\n" // rcx = arg1: stack_top
			  "mov %[boot], %%rdi\n"  // rdx = arg2: boot_info, becomes kernel SysV
											  // 1st arg
			  "jmp *%[kernel]\n"		  // r8 = arg3: kernel entry virtual address
			  :
			  : [stack] "r"(stack_top), [boot] "r"((void *)&bi), [kernel] "r"(kbuf)
			  : "memory");

	// Print(u"Shouhdn't reach here...");
	while (true) {
		__asm__ volatile("hlt");
	}

	return EFI_SUCCESS;
}