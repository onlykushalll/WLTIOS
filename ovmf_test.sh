#!/bin/bash
# A.4: Real OVMF boot test. Convert wlt-boot.efi to PE/COFF + run under QEMU UEFI.
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
WORK=/tmp/ovmf-test
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

rm -rf "$WORK"; mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/boot/keys"
cd "$WORK"

echo "=== 1. Convert ELF to PE/COFF (UEFI requires PE) ==="
objcopy --target=efi-app-x86_64 "$BOOTDIR/wlt-boot.efi" esp/EFI/BOOT/BOOTX64.EFI 2>&1 || {
    echo "objcopy efi-app failed, trying pe-x86-64..."
    objcopy --target=pe-x86-64 -O binary "$BOOTDIR/wlt-boot.efi" esp/EFI/BOOT/BOOTX64.EFI 2>&1
}
file esp/EFI/BOOT/BOOTX64.EFI 2>/dev/null
ls -la esp/EFI/BOOT/BOOTX64.EFI

echo ""
echo "=== 2. Create a minimal stub kernel (to test the handoff) ==="
# A tiny freestanding ELF64 that writes to serial port 0x3F8 then halts.
cat > stub.c << 'STUB'
/* Minimal stub kernel: writes "STUB KERNEL ALIVE" to serial, then halts.
 * This proves the bootloader's ExitBootServices + jump actually transferred
 * control to the kernel entry point. */
__asm__(".global _start\n_start:");
static void serial_out(const char *s) {
    volatile unsigned char *port = (unsigned char *)0x3F8;
    while (*s) {
        *port = (unsigned char)*s;
        s++;
    }
}
void _start(void) {
    serial_out("STUB KERNEL ALIVE\n");
    __asm__ __volatile__("hlt");
    for (;;) {}
}
STUB
gcc -ffreestanding -nostdlib -static -fno-pie -fno-stack-protector \
    -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
    -Wl,--entry=_start -Wl,-Ttext=0x100000 \
    -o stub_kernel.elf stub.c 2>&1
file stub_kernel.elf
echo "Stub entry: $(readelf -h stub_kernel.elf | grep Entry)"

echo ""
echo "=== 3. Sign the stub kernel (Ed25519) ==="
# Generate a keypair + sign the kernel using the project's monocypher
# For now, use a test key (the bootloader's config has verify_signature)
# Actually, let's disable signature verification for the test by setting
# wltios.conf with verify_signature=false
cat > esp/boot/wltios.conf << 'CONF'
kernel_path=\\boot\\sel4.bin
verify_signature=false
timeout_ms=3000
CONF

# Copy stub kernel as sel4.bin
cp stub_kernel.elf esp/boot/sel4.bin

echo ""
echo "=== 4. Build ESP FAT image ==="
# Create a FAT16 disk image with the ESP contents
dd if=/dev/zero of=esp.img bs=1M count=16 2>/dev/null
mkfs.vfat -F 16 -n ESP esp.img 2>/dev/null
mcopy -i esp.img -s esp/* :: 2>/dev/null || {
    echo "mcopy not available, trying mtools..."
    apt list --installed 2>/dev/null | grep mtools
}
ls -la esp.img

echo ""
echo "=== 5. Copy OVMF vars (writable copy) ==="
cp "$OVMF_VARS" ovmf_vars.fd

echo ""
echo "=== 6. Boot under OVMF (QEMU UEFI) ==="
timeout 30 qemu-system-x86_64 \
    -machine q35,accel=kvm \
    -cpu host \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive format=raw,file=esp.img \
    -serial mon:stdio \
    -display none \
    -no-reboot 2>&1 | tee ovmf_serial.log | head -80

echo ""
echo "=== 7. Check serial log for boot messages ==="
echo "--- WLTIOS Boot Loader messages ---"
grep -i "WLTIOS\|Boot Loader\|ExitBootServices\|Booting seL4\|STUB KERNEL" ovmf_serial.log | head -10
echo "--- OVMF errors ---"
grep -i "error\|fail\|panic" ovmf_serial.log | head -5
