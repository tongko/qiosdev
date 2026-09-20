#include <efiglobal.h>
#include <elf64.h>
#include <memmap.h>
#include <paging.h>
#include <efi.h>
#include <efilib.h>

static EFI_GUID _fs_protocol_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

static UINT64 elfflags_to_paging_attr(UINT32 p_flags) {
	// Every valid segment must at least be marked as Present
	UINT64 paging_attributes = PAGE_PRESENT;

	// 1. Handle Writable Permission
	if (p_flags & PF_W) {
		paging_attributes |= PAGE_WRITE;
	}

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_ELF)
	Print(u"[DEBUG] _nxe_enabled is: %s\r\n", (_nxe_enabled && (p_flags & PF_X)) ? u"true" : u"false");
#endif
	//  2. Handle Executable Permission (Invert for x86_64 NX)
	if (_nxe_enabled) {
		if (!(p_flags & PF_X)) {
			paging_attributes |= PAGE_NX;
		} else {
			paging_attributes &= ~PAGE_NX;
		}
	}

	// Leave PAGE_USER unset. After transitions to user space later,
	// kernel can update those specific page tables itself.

	return paging_attributes;
}

/*****************************************************************************
 * function: open_file
 * Try to open file from any SimpleFileSystem volume. Returns EFI_SUCCESS and
 * out_file if found.
 ******************************************************************************/
EFI_STATUS open_file(IN const CHAR16 *fname, OUT EFI_FILE_PROTOCOL **out_file) {
	EFI_HANDLE *hbuffer = NULL;
	UINTN count = 0;
	CHAR16 *fp = (CHAR16 *)fname;
	UINTN i;

	*out_file = NULL;
	Print(u"[open_file] Get all handles with SimpleFileSystem protocol... ");
	// Get all handles with SimpleFileSystem protocol
	EFI_STATUS status = BS->LocateHandleBuffer(ByProtocol, &_fs_protocol_guid, NULL, &count, &hbuffer);
	if (EFI_ERROR(status)) {
		Print(u"\r\n%E❌ [open_file] Locate handle buffer failed: %r%N\r\n", status);
		return status;
	}
	Print(u"done, found %d.\r\n", count);

	// Iterate each volume handle, try open /sys/qios.elf
	bool failed = false;
	for (i = 0; i < count; i++) {
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
		EFI_FILE_PROTOCOL *root = NULL;

		Print(u"[open_file] Opening simple file system protocol... ");
		status =
			BS->OpenProtocol(hbuffer[i], &_fs_protocol_guid, (VOID **)&fs, NULL, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(status)) {
			if (!failed) {
				Print(u"\r\n");
			}
			Print(u"%E⚠️ [open_file] Open simple file system protocol failed: "
					u"%r%N\r\n",
					status);
			failed = true;
			continue;
		}
		Print(u"done.\r\n[open_file] Open volume's root directory... ");

		// Open volume root director
		status = fs->OpenVolume(fs, &root);
		if (EFI_ERROR(status)) {
			if (!failed) {
				Print(u"\r\n");
			}
			Print(u"%E⚠️ [open_file] Open volume root directory failed: %r%N\r\n");
			failed = true;
			continue;
		}
		Print(u"done.\r\n[open_file] Try to open target file on this volume... ");

		// Try to open target file on this volume.
		status = root->Open(root, out_file, fp, EFI_FILE_MODE_READ, 0);
		if (EFI_ERROR(status)) {
			if (!failed) {
				Print(u"\r\n");
			}
			Print(u"%E⚠️ [open_file] Can't open file: %r%N\r\n", status);
			root->Close(root);
			continue;
		}
		Print(u"done.\r\n");

		// Close root dir, keep out_file if success
		root->Close(root);
		if (!EFI_ERROR(status)) {
			break; // Found target file on this volume
		}
	}

	BS->FreePool(hbuffer);
	if (*out_file == NULL) {
		return EFI_NOT_FOUND;
	}

	return EFI_SUCCESS;
}

/*****************************************************************************
 * function: load_elf
 * Load the file handle to memory based on ELF standard.
 ******************************************************************************/
EFI_STATUS load_elf(IN const EFI_FILE_HANDLE hfile, OUT UINTN *out_entry) {
	if (!hfile) {
		return EFI_INVALID_PARAMETER;
	}

	elf64_ehdr_t ehdr;
	UINTN hdr_sz = sizeof(elf64_ehdr_t);

	// Read the ELF identification header
	Print(u"[load_elf] Read ELF identification header... ");
	EFI_STATUS status = hfile->Read(hfile, &hdr_sz, &ehdr);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ Read failed.%r%N\r\n", status);
		return status;
	}
	Print(u"done.\r\n[load_elf] Validate ELF magic bytes... ");

	// Validate ELF magic bytes
	if (*(UINT32 *)ehdr.e_ident != ELF_MAGIC) {
		Print(u"%E❌ Invalid ELF magic: %X%N\r\n", *(UINT32 *)ehdr.e_ident);
		return EFI_LOAD_ERROR;
	}
	Print(u"done.\r\n[load_elf] Read program headers block... ");

	// Navigate file pointer context to read the Program Headers block
	UINTN hdr_buf_sz = ehdr.e_phnum * ehdr.e_phentsize;
	elf64_phdr_t *phdrs = AllocatePool(hdr_buf_sz);
	if (!phdrs) {
		Print(u"%E❌ Failed to allocate memory.%N\r\n");
		return EFI_OUT_OF_RESOURCES;
	}
	status = hfile->SetPosition(hfile, ehdr.e_phoff);
	if (EFI_ERROR(status)) {
		FreePool(phdrs);
		Print(u"%E❌ Can't set position of file handle: %r%N\r\n", status);
		return status;
	}
	status = hfile->Read(hfile, &hdr_buf_sz, phdrs);
	if (EFI_ERROR(status)) {
		FreePool(phdrs);
		Print(u"%E❌ Can't read program header table: %r%N\r\n", status);
		return status;
	}
	Print(u"done.\r\n[load_elf] Process each program header segment... ");

	// Process each Program Header segment
	for (UINT16 i = 0; i < ehdr.e_phnum; i++) {
		elf64_phdr_t *hdr = &phdrs[i];
		// Only parse segtments labeled as loadable code/data blocks (PT_LOAD)
		if (hdr->p_type != PT_LOAD || hdr->p_memsz == 0) {
			continue;
		}

		UINTN pg_cnt = (hdr->p_memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
		EFI_PHYSICAL_ADDRESS seg_dest = alloc_pages(AllocateAnyPages, EfiLoaderCode, pg_cnt);
		if (!seg_dest) {
			FreePool(phdrs);
#if defined(EFI_DEBUG) && defined(EFI_DEBUG_ELF)
			Print(u"[DEBUG] hdr->p_memsz: %lx\r\n", hdr->p_memsz);
			Print(u"[DEBUG] pg_cnt: %d\r\n", pg_cnt);
#endif
			Print(u"%E❌ allocate pages for segment %d failed.\r\n", i);
			return EFI_OUT_OF_RESOURCES;
		}
		Print(u"done.\r\n[load_elf] Load file segment bytes into newly allocated "
				u"buffer space... ");
		status = hfile->SetPosition(hfile, hdr->p_offset);
		if (EFI_ERROR(status)) {
			FreePool(phdrs);
			Print(u"%E❌ Can't set position of file handle for segment %d: %r%N\r\n", i, status);
			return status;
		}

		UINTN read_sz = hdr->p_filesz;
		status = hfile->Read(hfile, &read_sz, (void *)seg_dest);
		if (EFI_ERROR(status)) {
			FreePool(phdrs);
			Print(u"%E❌ Can't read segment %d: %r%N\r\n", i, status);
			return status;
		}

		// Zero-init any remaining padding space required by the segment size (e.g.
		// .bss sections)
		if (hdr->p_memsz > hdr->p_filesz) {
			UINT8 *bss_start = (UINT8 *)(seg_dest + hdr->p_filesz);
			UINTN bss_sz = hdr->p_memsz - hdr->p_filesz;
			SetMem(bss_start, bss_sz, 0);
		}
		Print(u"done.\r\n[load_elf] Mapping kernel into page table... ");

		// Map the kernel into page table.
		EFI_VIRTUAL_ADDRESS vaddr = hdr->p_vaddr;
		UINT64 attr = elfflags_to_paging_attr(hdr->p_flags);
#if defined(EFI_DEBUG) && defined(EFI_DEBUG_ELF)
		Print(u"\r\n[DEBUG] attribute is %lx\r\n", attr);
#endif
		status = map_virt_addr(vaddr, seg_dest, attr, hdr->p_memsz, PAGE_4K);
		if (EFI_ERROR(status)) {
			FreePool(phdrs);
#if defined(EFI_DEBUG) && defined(EFI_DEBUG_ELF)
			Print(u"[DEBUG] hdr->p_vaddr: 0x%lx\r\n", vaddr);
			Print(u"[DEBUG] attr: %lx\r\n", attr);
			Print(u"[DEBUG] hdr->m_memsz: %lx\r\n", hdr->p_memsz);
#endif
			Print(u"%E❌ Can't map setment %d to paging: %r%N\r\n", i, status);
			return status;
		}

		Print(u"done.\r\n");
	}

	// Free metadata trackers and extract runtime execution address pointer
	FreePool(phdrs);
	*out_entry = ehdr.e_entry;

	return EFI_SUCCESS;
}