#!/usr/bin/env bash

# Detect system OVMF paths dynamically
if [ -f "/usr/share/edk2/x64/OVMF_VARS.4m.fd" ]; then
	# Path configuration for Omarchy (Arch-based)
	OVMF_CODE="/usr/share/edk2/x64/OVMF_CODE.4m.fd"
	OVMF_VARS_TEMPLATE="/usr/share/edk2/x64/OVMF_VARS.4m.fd"
	echo "➡ Detected Architecture: Omarchy / Arch Linux"
elif [ -f "/usr/share/OVMF/OVMF_CODE_4M.fd" ]; then
	# Path config for Ubuntu / Debian
	OVMF_CODE="/usr/share/OVMF/OVMF_CODE_4M.fd"
	OVMF_VARS_TEMPLATE="/usr/share/OVMF/OVMF_VARS_4M.fd"
	echo "➡ Detected Architecture: Ubuntu / Debian"
elif [ -f "/usr/share/edk2/ovmf/OVMF_VARS.fd" ]; then
	# Path configuration for Fedora
	OVMF_CODE="/usr/share/edk2/ovmf/OVMF_CODE.fd"
	OVMF_VARS_TEMPLATE="/usr/share/edk2/ovmf/OVMF_VARS.fd"
	echo "➡ Detected Architecture: Fedora"
else
	echo "❌ Error: UEFI OVMF firmware package not found on this machine."
	echo "Please run: 'omarchy pkg add edk2-ovmf' or 'sudo apt install ovmf'"
	exit 1
fi

LOCAL_VARS="./MY_VARS.fd"
if [ ! -f "$LOCAL_VARS" ]; then
	echo "➡ Genrating fresh NVRAM variable file: $LOCAL_VARS"
	cp "$OVMF_VARS_TEMPLATE" "$LOCAL_VARS"
fi

echo "➡ Launching QEMU instance..."
# Check if the first parameter ($1) is empty
if [ -z "$1" ]; then
	qemu-system-x86_64 \
		-enable-kvm \
		-cpu host \
		-smp 4 \
		-m 4096M \
		-machine q35 \
		-display gtk \
		-drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
		-drive format=raw,file=build/esp.img,index=0,media=disk \
		-usb \
		-serial stdio
else
    qemu-system-x86_64 \
		-enable-kvm \
		-cpu host \
		-smp 4 \
		-m 4096M \
		-machine q35 \
		-display gtk \
		-drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
		-drive if=pflash,format=raw,file="$LOCAL_VARS" \
		-drive format=raw,file=build/esp.img,index=0,media=disk \
		-usb \
		-serial stdio \
		-s -S
fi

#		-device virtio-vga-gl -display sdl,gl=on \
