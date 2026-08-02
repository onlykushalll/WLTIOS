#!/bin/bash
# A.4: OVMF boot test with PE-converted bootloader.
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
WORK=/tmp/ovmf-test2
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

rm -rf "$WORK"; mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/boot/keys"
cd "$WORK"

echo "=== 1. Convert ELF to PE/COFF ==="
objcopy -O pei-x86-64 "$BOOTDIR/wlt-boot.efi" esp/EFI/BOOT/BOOTX64.EFI 2>&1
file esp/EFI/BOOT/BOOTX64.EFI
# Patch PE subsystem to 10 (EFI application) at offset 0x5C (Subsystem field in PE header)
python3 -c "
import struct
with open('esp/EFI/BOOT/BOOTX64.EFI', 'r+b') as f:
    # PE header offset is at offset 0x3C
    f.seek(0x3C)
    pe_off = struct.unpack('<I', f.read(4))[0]
    # Subsystem is at PE+0x5C (PE32+ optional header)
    sub_off = pe_off + 0x5C
    f.seek(sub_off)
    f.write(struct.pack('<H', 10))  # 10 = EFI Application
print('PE subsystem patched to 10 (EFI Application)')
"
file esp/EFI/BOOT/BOOTX64.EFI

echo ""
echo "=== 2. Create stub kernel ==="
cat > stub.c << 'STUB'
static void serial_out(const char *s) {
    volatile unsigned char *port = (unsigned char *)0x3F8;
    while (*s) { *port = (unsigned char)*s; s++; }
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
echo "Stub: $(file stub_kernel.elf)"

echo ""
echo "=== 3. Create wltios.conf (disable sig verify for test) ==="
cat > esp/boot/wltios.conf << 'CONF'
kernel_path=\\boot\\sel4.bin
verify_signature=false
timeout_ms=3000
CONF
cp stub_kernel.elf esp/boot/sel4.bin

echo ""
echo "=== 4. Build ESP FAT image ==="
dd if=/dev/zero of=esp.img bs=1M count=16 2>/dev/null
mkfs.vfat -F 16 -n ESP esp.img 2>/dev/null
mcopy -i esp.img -s esp/EFI ::
mcopy -i esp.img -s esp/boot ::
ls -la esp.img

echo ""
echo "=== 5. Boot under OVMF ==="
cp "$OVMF_VARS" ovmf_vars.fd
timeout 25 qemu-system-x86_64 \
    -machine q35,accel=kvm \
    -cpu host \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive format=raw,file=esp.img \
    -serial mon:stdio \
    -display none \
    -no-reboot 2>&1 | tee ovmf_serial.log | head -100

echo ""
echo "=== 6. Check serial log ==="
echo "--- Boot loader messages ---"
grep -i "WLTIOS\|Boot Loader\|ExitBootServices\|Booting seL4\|STUB KERNEL\|Signature\|Memory map\|Kernel loaded" ovmf_serial.log | head -15
echo "--- Errors ---"
grep -i "error\|fail\|panic\|refused\|cannot" ovmf_serial.log | head -5
echo "--- Full log size ---"
wc -l ovmf_serial.log
