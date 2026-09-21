#include <bootinfo.h>
#include <efi.h>
#include <efilib.h>

bootinfo_t *_bi;
static EFI_GUID _gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

EFI_STATUS bootinfo_init(bootinfo_t *out_bi) {
	// 1. Should already have memmap, pml4, etc.
	// need to obtain framebuffer
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

	// Search graphics handle globally
	Print(u"[bootinfo_init] Searching for graphics handle... ");
	EFI_STATUS status = BS->LocateProtocol(&_gop_guid, NULL, (VOID **)&gop);

	if (EFI_ERROR(status) || !gop) {
		// If it returns and error, something is fundamentally wrong with the
		// display
		Print(u"%E❌ [bootinfo_init] Could not locate graphics handle: %r%N\r\n", status);
		return status;
	}
	Print(u"done.\r\n");

	UINTN szc = 0;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *infoc = NULL;
	gop->QueryMode(gop, gop->Mode->Mode, &szc, &infoc);
	Print(u"[bootinfo_init] Current graphics mode: fmt=%d,Hpx=%d,Vpx=%d\r\n",
			infoc->PixelFormat,
			infoc->HorizontalResolution,
			infoc->VerticalResolution);

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_GOP)
	Print(u"Enumerating all output mode:\r\n");
#endif
	UINTN i;
	for (i = 0; i < gop->Mode->MaxMode; i++) {
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
		UINTN sz = 0;
		if (EFI_ERROR(gop->QueryMode(gop, i, &sz, &info))) {
			continue;
		}

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_GOP)
		Print(u"Version          : %u\r\n", info->Version);
		Print(u"Horizontal Res   : %u\r\n", info->HorizontalResolution);
		Print(u"Vertical Res     : %u\r\n", info->VerticalResolution);
		Print(u"Pixel Format     : %d\r\n", info->PixelFormat);
		Print(u"PixelInformation : %d\r\n", info->PixelInformation);
		Print(u"PixelsPerScanLine: %d\r\n", info->PixelsPerScanLine);
#endif

		// Will will default the resolution to 1024x768
		//	TODO: set fallback if not found
		if (info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor && info->HorizontalResolution == 1920 &&
			 info->VerticalResolution == 1080) {
			// Activate graphics mode
			Print(u"[bootinfo_init] Set graphics mode to index %d\r\n", i);
			gop->SetMode(gop, i);
			break;
		}
	}

	if (i >= gop->Mode->MaxMode) {
		Print(u"%E❌ [bootinfo_init] No graphics mode!%N\r\n");
	}

	if (gop && gop->Mode) {
		out_bi->frame_buff.base_addr = (UINT32 *)gop->Mode->FrameBufferBase;
		out_bi->frame_buff.size = gop->Mode->FrameBufferSize;
		out_bi->frame_buff.width = gop->Mode->Info->HorizontalResolution;
		out_bi->frame_buff.height = gop->Mode->Info->VerticalResolution;
		out_bi->frame_buff.px_per_scanline = gop->Mode->Info->PixelsPerScanLine;
		out_bi->frame_buff.px_format = gop->Mode->Info->PixelFormat;
	}

	return EFI_SUCCESS;
}

static void bubble_sort(allocated_t arr[], int size) {
	for (int i = 0; i < size - 1; i++) {
		for (int j = 0; j < size - i - 1; j++) {
			// Swap if the current element is bigger than the next
			if (arr[j].pstart > arr[j + 1].pstart && arr[j + 1].pstart > 0) {
				allocated_t temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
}

static void merge_alloc(allocated_t arr[], int size) {
	int i = size - 1;
	while (i) {
		if (arr[i].pstart == arr[i - 1].pend) {
			// Contiguos, merge
			arr[i - 1].pend = arr[i].pend;
			arr[i].pstart = arr[i].pend = 0;
		}
		i--;
	}

	for (i = 0; i < size; i++) {
		if (arr[i].pstart == 0 && arr[i].pend == 0) {
#if defined(EFI_DEBUG) && defined(EFI_DEBUG_BI)
			Print(u"[DEBUG] index %d is zero.\r\n", i);
#endif
			continue;
		}

		if (i && (arr[i - 1].pstart == 0 && arr[i - 1].pend == 0) && (arr[i].pstart)) {
			int j = i;
			while (j) {
				if (arr[j - 1].pstart == 0 && arr[j - 1].pend == 0) {
					arr[j - 1].pstart = arr[j].pstart;
					arr[j - 1].pend = arr[j].pend;
					arr[j].pstart = arr[j].pend = 0;
				} else {
					break;
				}
				j--;
			}
		}
	}
}

void reorder_alloc(bootinfo_t *bi) {
	if (!bi) {
		return;
	}

	bubble_sort(bi->alloc_pages, 20);
	merge_alloc(bi->alloc_pages, 20);
}