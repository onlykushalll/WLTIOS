#!/bin/bash
# Create FAT filesystem at offset in GPT disk using mtools (no loop device needed).
set -e
cd /tmp/ovmf-test4

echo "=== Create GPT disk ==="
dd if=/dev/zero of=disk.img bs=1M count=64 2>/dev/null
sgdisk --clear --new=1:2048:0 --typecode=1:EF00 --change-name=1:ESP disk.img 2>/dev/null
sgdisk -p disk.img 2>&1 | tail -3

echo ""
echo "=== Create FAT32 at partition offset (sector 2048 = byte 1048576) ==="
# Create a separate FAT image, then dd it into the disk at the right offset
dd if=/dev/zero of=fat.img bs=1M count=32 2>/dev/null
mkfs.vfat -F 32 -n ESP fat.img 2>/dev/null
mcopy -i fat.img -s esp/EFI ::
mcopy -i fat.img -s esp/boot ::
mdir -i fat.img ::/EFI/BOOT ::/boot 2>&1 | head -15

echo ""
echo "=== Write FAT into disk partition ==="
# Partition starts at sector 2048 = byte 1048576
dd if=fat.img of=disk.img bs=1M seek=1 conv=notrunc 2>&1 | tail -1

echo ""
echo "=== Verify disk structure ==="
# Check if the FAT is readable from the disk at the offset
mdir -i disk.img@@1048576 ::/EFI/BOOT 2>&1 | head -10

echo ""
echo "=== Boot under OVMF (multiple attempts) ==="
cp /usr/share/OVMF/OVMF_VARS_4M.fd ovmf_vars.fd

echo "--- Attempt A: q35 + virtio ---"
timeout 20 qemu-system-x86_64 -machine q35 -accel kvm -cpu host -m 512M \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive file=disk.img,format=raw,if=virtio \
    -serial mon:stdio -display none -no-reboot > ovmf_a.log 2>&1
strings ovmf_a.log | grep -v "^\[" | grep -v "^$" | head -10

echo ""
echo "--- Attempt B: q35 + ahci (SATA) ---"
cp /usr/share/OVMF/OVMF_VARS_4M.fd ovmf_vars.fd
timeout 20 qemu-system-x86_64 -machine q35 -accel kvm -cpu host -m 512M \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive file=disk.img,format=raw,if=none,id=disk0 \
    -device ahci,id=ahci0 \
    -device ide-hd,drive=disk0,bus=ahci0.0 \
    -serial mon:stdio -display none -no-reboot > ovmf_b.log 2>&1
strings ovmf_b.log | grep -v "^\[" | grep -v "^$" | head -10

echo ""
echo "--- Attempt C: pc + IDE (hda) ---"
cp /usr/share/OVMF/OVMF_VARS_4M.fd ovmf_vars.fd
timeout 20 qemu-system-x86_64 -machine pc -accel kvm -cpu host -m 512M \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive file=disk.img,format=raw,if=ide \
    -serial mon:stdio -display none -no-reboot > ovmf_c.log 2>&1
strings ovmf_c.log | grep -v "^\[" | grep -v "^$" | head -10

echo ""
echo "=== Results ==="
for f in ovmf_a.log ovmf_b.log ovmf_c.log; do
    echo "--- $f ---"
    if grep -q "WLTIOS\|Boot Loader\|ExitBoot\|STUB" "$f" 2>/dev/null; then
        echo "BOOTLOADER RAN!"
        grep -i "WLTIOS\|Boot Loader\|ExitBoot\|STUB\|error\|fail\|Booting" "$f" | head -10
    elif grep -q "BdsDxe.*failed" "$f" 2>/dev/null; then
        echo "OVMF found disk but couldn't boot"
        grep "BdsDxe" "$f" | head -3
    else
        echo "No boot attempt detected"
        strings "$f" | grep -v "^\[" | grep -v "^$" | head -5
    fi
done
