#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════
# boot-terminal-fix.sh — Boot WLTIOS with corrected wlt-terminal (kernel font_8x16)
#
# Purpose: Boot QEMU with the fixed terminal binary, capture screenshot,
#          convert to PNG. Used to verify the font fix.
#
# Usage (from WSL2):
#   bash /mnt/c/.../WLTIOS/scripts/boot-terminal-fix.sh
#
# Prerequisites:
#   - wlt-term-wayland.c compiled via build-all.sh
#   - Binary installed at ~/wltios-build/wltios-rootfs64/usr/bin/wlt-terminal
#   - Initramfs repacked (with fakeroot for /dev char nodes)
#
# Output:
#   /home/kushal/wlt-terminal-fix-serial.log  (serial console)
#   /home/kushal/wlt-terminal-fix.ppm          (raw screenshot)
#   /home/kushal/wlt-terminal-fix.png          (converted PNG)
# ═══════════════════════════════════════════════════════════════════════
set -e

LOG=/home/kushal/wlt-terminal-fix-serial.log
MON=/home/kushal/wlt-terminal-fix-mon.sock
SHOT=/home/kushal/wlt-terminal-fix.ppm
PNG=/home/kushal/wlt-terminal-fix.png

pkill -f qemu-system-x86 2>/dev/null || true
sleep 2
rm -f "$LOG" "$MON" "$SHOT" "$PNG"

echo "=== Launching QEMU (virtio-vga, 1280x800) ==="
nohup qemu-system-x86_64 \
    -kernel /home/kushal/wltios-build/kernel-amd64 \
    -initrd /home/kushal/wltios-build/wltios-initramfs64.cpio.gz \
    -append 'console=ttyS0 root=/dev/ram rdinit=/init' \
    -m 512 -no-reboot \
    -netdev user,id=n0 -device e1000,netdev=n0 \
    -device virtio-vga -display none \
    -serial file:"$LOG" \
    -monitor unix:"$MON",server,nowait \
    > /home/kushal/wlt-terminal-fix-stderr.log 2>&1 &
QPID=$!
sleep 3
kill -0 $QPID 2>/dev/null && echo "QEMU_ALIVE (pid=$QPID)" || { echo "QEMU_DIED"; cat /home/kushal/wlt-terminal-fix-stderr.log; exit 1; }

echo "=== Waiting 20s for boot ==="
sleep 50

echo "=== Serial log size ==="
wc -c "$LOG"
echo ""
echo "=== wlt-term messages ==="
grep -iE "wlt-term|render_grid|drew" "$LOG" | tail -15
echo ""
echo "=== Serial tail ==="
tail -15 "$LOG"

echo ""
echo "=== Capture screenshot ==="
echo "screendump $SHOT" | nc -U -q1 "$MON" 2>/dev/null
sleep 3
ls -la "$SHOT" 2>&1

echo ""
echo "=== Convert PPM to PNG ==="
if [ -f "$SHOT" ]; then
    python3 << 'PYEOF'
import struct, zlib, re
with open("/home/kushal/wlt-terminal-fix.ppm","rb") as f: raw=f.read()
p6=raw.find(b"P6\n")
hdr=raw[p6:]
m=re.search(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", hdr)
w,h,mx=int(m.group(1)),int(m.group(2)),int(m.group(3))
data=hdr[m.end():]
print(f"Screenshot: {w}x{h}, {len(data)} bytes")
green=sum(1 for i in range(0,len(data),3) if data[i+1]>150 and data[i]<100 and data[i+2]<100)
print(f"Green pixels: {green}")
def chunk(t,d):
    c=t+d
    return struct.pack(">I",len(d))+c+struct.pack(">I",zlib.crc32(c)&0xffffffff)
raw_png=bytearray()
for y in range(h):
    raw_png.append(0)
    raw_png.extend(data[y*w*3:(y+1)*w*3])
with open("/home/kushal/wlt-terminal-fix.png","wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
    f.write(chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0)))
    f.write(chunk(b"IDAT",zlib.compress(bytes(raw_png))))
    f.write(chunk(b"IEND",b""))
print(f"PNG saved: /home/kushal/wlt-terminal-fix.png")
PYEOF
fi

echo ""
echo "=== Kill QEMU ==="
kill $QPID 2>/dev/null || true
echo "=== DONE ==="
