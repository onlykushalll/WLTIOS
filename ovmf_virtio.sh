#!/bin/bash
# Try virtio-blk + also try OVMF shell as fallback.
set -e
cd /tmp/ovmf-test4

echo "=== ATTEMPT 1: virtio-blk ==="
timeout 20 qemu-system-x86_64 \
    -machine q35 -accel kvm -cpu host \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive file=disk.img,format=raw,if=virtio \
    -serial mon:stdio -display none -no-reboot > ovmf_virtio.log 2>&1
echo "EXIT=$?"
echo "--- virtio log ---"
strings ovmf_virtio.log | grep -v "^\[" | grep -v "^$" | head -15

echo ""
echo "=== ATTEMPT 2: OVMF shell (copy shell to ESP, boot from it) ==="
# Find OVMF shell
SHELL=$(find /usr -name Shell_*_4M.efi 2>/dev/null | head -1)
if [ -z "$SHELL" ]; then
    SHELL=$(find /usr -name Shell.efi 2>/dev/null | head -1)
fi
echo "OVMF shell: $SHELL"

# Rebuild FAT with shell + our bootloader
cp "$SHELL" esp/EFI/BOOT/SHELLX64.EFI 2>/dev/null || echo "No shell found"
# Also put startup.nsh to auto-run our bootloader
printf 'BOOTX64.EFI\n' > esp/startup.nsh

# Rebuild FAT image
dd if=/dev/zero of=fat.img bs=1M count=32 2>/dev/null
mkfs.vfat -F 32 -n ESP fat.img 2>/dev/null
mcopy -i fat.img -s esp/EFI ::
mcopy -i fat.img -s esp/boot ::
mcopy -i fat.img -s esp/startup.nsh ::

# Rebuild GPT disk
dd if=/dev/zero of=disk.img bs=1M count=64 2>/dev/null
sgdisk --clear --new=1:2048:0 --typecode=1:EF00 --change-name=1:ESP disk.img 2>/dev/null
dd if=fat.img of=disk.img bs=512 seek=2048 conv=notrunc 2>/dev/null

# Try with -hda (raw IDE, no format restriction)
cp /usr/share/OVMF/OVMF_VARS_4M.fd ovmf_vars.fd
timeout 25 qemu-system-x86_64 \
    -machine q35 -accel kvm -cpu host \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive file=disk.img,format=raw,if=virtio \
    -serial mon:stdio -display none -no-reboot > ovmf_shell.log 2>&1
echo "EXIT=$?"
echo "--- shell log ---"
strings ovmf_shell.log | grep -v "^\[" | grep -v "^$" | head -20
