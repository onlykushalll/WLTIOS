#!/bin/bash
# A.4: OVMF boot test v4 — FAT32 + larger disk + verify partition contents.
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
WORK=/tmp/ovmf-test4
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

rm -rf "$WORK"; mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/boot"
cd "$WORK"

echo "=== 1. Create PE bootloader ==="
objcopy -O pei-x86-64 "$BOOTDIR/wlt-boot.efi" bootx64.efi 2>&1
python3 -c "
import struct
with open('bootx64.efi', 'r+b') as f:
    f.seek(0x3C); pe_off = struct.unpack('<I', f.read(4))[0]
    f.seek(pe_off + 0x5C); f.write(struct.pack('<H', 10))
"
file bootx64.efi

echo "=== 2. Stub kernel ==="
cat > stub.c << 'STUB'
static void serial_out(const char *s) {
    volatile unsigned char *p = (unsigned char *)0x3F8;
    while (*s) { *p = *s; s++; }
}
void _start(void) {
    serial_out("STUB KERNEL ALIVE\n");
    __asm__ __volatile__("hlt");
    for (;;) {}
}
STUB
gcc -ffreestanding -nostdlib -static -fno-pie -Wl,--entry=_start -Wl,-Ttext=0x100000 \
    -mno-red-zone -mno-sse -o sel4.bin stub.c 2>&1
file sel4.bin

echo "=== 3. Create FAT32 ESP image (32MB, proper size for FAT32) ==="
cp bootx64.efi esp/EFI/BOOT/BOOTX64.EFI
cp sel4.bin esp/boot/sel4.bin
printf 'kernel_path=\\boot\\sel4.bin\nverify_signature=false\ntimeout_ms=3000\n' > esp/boot/wltios.conf

# FAT32 needs at least ~33MB for default cluster size, but can work with smaller
dd if=/dev/zero of=fat.img bs=1M count=32 2>/dev/null
mkfs.vfat -F 32 -n ESP fat.img 2>/dev/null
mcopy -i fat.img -s esp/EFI ::
mcopy -i fat.img -s esp/boot ::
echo "FAT contents:"
mdir -i fat.img ::/EFI/BOOT ::/boot 2>&1

echo "=== 4. Create GPT disk (64MB, ESP partition) ==="
dd if=/dev/zero of=disk.img bs=1M count=64 2>/dev/null
sgdisk --clear --new=1:2048:0 --typecode=1:EF00 --change-name=1:ESP disk.img 2>&1 | tail -1
# Write FAT into partition (sector 2048)
dd if=fat.img of=disk.img bs=512 seek=2048 conv=notrunc 2>&1 | tail -1
sgdisk -p disk.img 2>&1 | tail -3

echo "=== 5. Boot under OVMF ==="
cp "$OVMF_VARS" ovmf_vars.fd
timeout 30 qemu-system-x86_64 \
    -machine pc -accel kvm -cpu host \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file=ovmf_vars.fd \
    -drive format=raw,file=disk.img,if=ide \
    -serial mon:stdio -display none -no-reboot > ovmf_serial.log 2>&1
echo "EXIT=$?"

echo ""
echo "=== 6. Serial log ==="
wc -c ovmf_serial.log
echo "--- Key messages ---"
strings ovmf_serial.log | grep -i "WLTIOS\|Boot Loader\|ExitBootServices\|STUB\|Booting\|kernel\|Memory\|loaded\|BdsDxe\|error\|fail\|Not Found" | head -20
echo "--- All non-empty ---"
strings ovmf_serial.log | grep -v "^\[" | grep -v "^$" | head -30
