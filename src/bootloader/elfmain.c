#include <efimain.h>
#include <efiglobal.h>
#include <bootinfo.h>
#include <memmap.h>
#include <paging.h>
#include <extfs.h>
#include <elf64.h>
#include <efi.h>
#include <efilib.h>

#define COLOR_ARGB(a, r, g, b)                                                 \
	(((UINT32)(a) << 24) | ((UINT32)(r) << 16) | ((UINT32)(g) << 8) | (UINT32)(b))

#define IA32_EFER_MSR 0xC0000080
#define IA32_EFER_NXE (1ULL << 11)
#define READ_EFER_MSR(high, low)                                               \
	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(IA32_EFER_MSR))

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
	_himage = ih; // set as global variable

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

	Print(u"[DEBUG] FB paddr: 0x%lx\r\n", bi.frame_buff.base_addr);

	Print(u"[efi_main] Exiting boot service... ");
	do {
		EFI_MEMORY_DESCRIPTOR *map;
		UINTN msz, dsz, key;
		BS->FreePool(bi.mem_map.map);
		status = get_memmap(&map, &msz, &dsz, &key);
		if (EFI_ERROR(status)) {
			Print(u"❌");
			return status;
		}
		Print(u" .");
		bi.mem_map.map = map;
		bi.mem_map.map_size = msz;
		bi.mem_map.desc_size = dsz;
		status = BS->ExitBootServices(_himage, key);
		if (status != EFI_SUCCESS && status != EFI_INVALID_PARAMETER) {
			Print(u"%E❌ [efi_main] ExitBootServices failed: %r%N\r\n", status);
			return status;
		}
	} while (status == EFI_INVALID_PARAMETER);

	UINT32 color = COLOR_ARGB(0, 40, 50, 60);
	for (UINTN y = 100; y < 200; y++) {
		for (UINTN x = 100; x < 200; x++) {
			bi.frame_buff.base_addr[(y * bi.frame_buff.px_per_scanline) + x] = color;
		}
	}

	SET_CR3(bi.pml4_paddr);
	kernel_entry_fn_t kernel_fn = (kernel_entry_fn_t)kbuf;
	kernel_fn(&bi);

	Print(u"Shouhdn't reach here...");
	while (true) {
		__asm__ volatile("hlt");
	}

	return EFI_SUCCESS;
}