#include <extfs.h>
#include <efiglobal.h>
#include <efi.h>
#include <efilib.h>

static EFI_GUID _loaded_img_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID _dev_path_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;

/*****************************************************************************
 * function: get_vol_dev_path
 * Get default (ESP) volume device path
 ******************************************************************************/
static EFI_STATUS get_vol_dev_path(EFI_DEVICE_PATH_PROTOCOL **out_dp) {
	EFI_LOADED_IMAGE_PROTOCOL *lip = NULL;

	*out_dp = NULL;
	// Open LoadedImage protocol
	EFI_STATUS status =
		BS->OpenProtocol(_himage, &_loaded_img_guid, (VOID **)&lip, _himage, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		Print(u"\r\n%E❌ [get_vol_dev_path] Open loaded image protocol failed: "
				u"%r%N\r\n",
				status);
		return status;
	}

	status = BS->OpenProtocol(
		lip->DeviceHandle, &_dev_path_guid, (VOID **)out_dp, _himage, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		Print(u"\r\n%E❌ [get_vol_dev_path] Open loaded image device path failed: "
				u"%r%N\r\n",
				status);
	}

	return status;
}

/*****************************************************************************
 * function: extfs_init
 * Initialize ExtFS - load from ESP volume DRIVERS folder
 ******************************************************************************/
EFI_STATUS extfs_init(const CHAR16 *fname) {
	EFI_DEVICE_PATH_PROTOCOL *vol_dp = NULL;
	EFI_DEVICE_PATH_PROTOCOL *drv_dp = NULL;
	EFI_HANDLE hdrv_img = NULL;

	Print(u"[extfs_init] Get device path of ESP volume... ");
	EFI_STATUS status = get_vol_dev_path(&vol_dp);
	if (EFI_ERROR(status)) {
		Print(u"\r\n%E❌ [extfs_init] Get device path of ESP volume failed: %r%N\r\n", status);
		return status;
	}
	Print(u"done.\r\n");

	// Append file node: \EFI\BOOT\DRIVERS\ext4_x64.efi
	// build device-path to ext4_x64.efi
	CHAR16 *drv_fp = (CHAR16 *)fname; // u"\\EFI\\BOOT\\DRIVERS\\ext4_x64.efi";
	Print(u"Append file node: '%s'... ", drv_fp);
	drv_dp = AppendDevicePath(vol_dp, FileDevicePath(NULL, drv_fp));
	if (!drv_dp) {
		Print(u"\r\n%E❌ [extfs_init] AppendDevicePath failed.%N\r\n");
		return EFI_NOT_FOUND;
	}
	Print(u"done.\r\n[extfs_init] Load driver image... ");

	status = BS->LoadImage(FALSE, _himage, drv_dp, NULL, 0, &hdrv_img);
	if (EFI_ERROR(status)) {
		BS->FreePool(drv_dp);
		Print(u"\r\n%E❌ [extfs_init] Load driver image failed: %r%N\r\n", status);
		return status;
	}
	Print(u"done.\r\n[extfs_init] Start image... ");

	// Start image: run driver entry point, registers DriveBinding protocol
	status = BS->StartImage(hdrv_img, NULL, NULL);
	if (EFI_ERROR(status)) {
		BS->FreePool(drv_dp);
		Print(u"\r\n%E❌ [extfs_init] Start driver image failed: %r%N\r\n", status);
		return status;
	}
	Print(u"done.\r\n[extfs_init] Trigger driver binding to existing disk "
			u"controllers.\r\n");
	// Iterate all handles that have DevicePath, call ConnectController on each
	UINTN count = 0;
	EFI_HANDLE *hbuf = NULL;
	Print(u"[extfs_init] Locate handle_buffer... ");
	status = BS->LocateHandleBuffer(ByProtocol, &_dev_path_guid, NULL, &count, &hbuf);
	if (EFI_ERROR(status)) {
		BS->FreePool(drv_dp);
		Print(u"\r\n%E❌ [extfs_init] Locate handle buffer failed: %r%N\r\n", status);
		return status;
	}
	Print(u"done. Found %d.\r\n", count);

	Print(u"Probing each controller recursively... ");
	for (UINTN i = 0; i < count; i++) {
		// Connect each controller recursively
		status = BS->ConnectController(hbuf[i], NULL, NULL, TRUE);
		if (EFI_ERROR(status) && status != EFI_NOT_FOUND) {
			Print(u"⚠️ [extfs_init]: Connect controller[%d] failed.\r\n", i);
		}
	}

	BS->FreePool(drv_dp);
	Print(u"ExtFS driver loaded.\r\n");

	return EFI_SUCCESS;
}