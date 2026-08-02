#!/bin/bash
# Create FAT filesystem directly inside GPT partition via loop device.
set -e
cd /tmp/ovmf-test4

echo "=== Create GPT disk + partition ==="
dd if=/dev/zero of=disk.img bs=1M count=64 2>/dev/null
sgdisk --clear --new=1:2048:0 --typecode=1:EF00 --change-name=1:ESP disk.img 2>/dev/null
sgdisk -p disk.img 2>&1 | tail -3

echo "=== Mount partition via loop ==="
# Use losetup with partition scanning
LOOPDEV=$(losetup -f --show -P disk.img 2>/dev/null)
echo "Loop device: $LOOPDEV"
ls -la ${LOOPDEV}* 2>/dev/null

if [ -n "$LOOPDEV" ]; then
    PART=${LOOPDEV}p1
    [ -b "$PART" ] || PART=${LOOPDEV}1
    echo "Partition: $PART"
    
    echo "=== Format partition as FAT32 ==="
    mkfs.vfat -F 32 -n ESP "$PART" 2>&1
    
    echo "=== Copy files ==="
    mkdir -p /tmp/mnt-esp
    mount "$PART" /tmp/mnt-esp 2>&1
    mkdir -p /tmp/mnt-esp/EFI/BOOT /tmp/mnt-esp/boot
    cp bootx64.efi /tmp/mnt-esp/EFI/BOOT/BOOTX64.EFI
    cp sel4.bin /tmp/mnt-esp/boot/sel4.bin
    printf 'kernel_path=\\boot\\sel4.bin\nverify_signature=false\ntimeout_ms=3000\n' > /tmp/mnt-esp/boot/wltios.conf
    
    echo "=== Verify files ==="
    ls -la /tmp/mnt-esp/EFI/BOOT/ /tmp/mnt-esp/boot/
    
    umount /tmp/mnt-esp 2>/dev/null
    losetup -d "$LOOPDEV" 2>/dev/null
    echo "Loop detached"
fi

echo ""
echo "=== Boot under OVMF (virtio) ==="
cp /usr/share/OVMF/OVMF_VARS_4M.fd ovmf_vars.fd
timeout 30 qemu-system-x86_64 \
    -machine q35 -accel kvm -cpu host \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive file=disk.img,format=raw,if=virtio \
    -serial mon:stdio -display none -no-reboot > ovmf_loop.log 2>&1
echo "EXIT=$?"

echo ""
echo "=== Serial log ==="
wc -c ovmf_loop.log
strings ovmf_loop.log | grep -v "^\[" | grep -v "^$" | head -30
