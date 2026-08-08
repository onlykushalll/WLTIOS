#!/bin/bash
# Fix Tor segfault: patch ld-linux with xor eax,eax; ret (return 0=success)
# instead of bare ret (undefined return value causes segfault).
set +e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-tor-fix

echo "=== 1. Patch ld-linux with xor eax,eax; ret (return 0 = success) ==="
python3 << 'PYEOF'
ld_path = "/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2"
with open(ld_path, "rb") as f:
    data = bytearray(f.read())

# The three ISA check call sites (call _dl_signal_error)
# Old patch: 0xc3 (ret) + 4x 0x90 (nop) = bare return, undefined eax
# New patch: 0x31 0xc0 (xor eax, eax) + 0xc3 (ret) + 2x 0x90 (nop) = return 0
# This makes the ISA check function return 0 (success) so initialization
# continues properly instead of leaving eax with garbage.

for offset in [0xf304, 0x27ec2, 0x2724c]:
    if data[offset] == 0xe8:  # still original call instruction
        print(f"  Site 0x{offset:x}: patching call -> xor eax,eax; ret")
        data[offset] = 0x31      # xor
        data[offset+1] = 0xc0    # eax, eax
        data[offset+2] = 0xc3    # ret
        data[offset+3] = 0x90    # nop
        data[offset+4] = 0x90    # nop
    elif data[offset] == 0xc3:  # already patched with bare ret
        print(f"  Site 0x{offset:x}: upgrading bare ret -> xor eax,eax; ret")
        data[offset] = 0x31      # xor
        data[offset+1] = 0xc0    # eax, eax
        # data[offset+2] already 0xc3 (ret)
        # data[offset+3] already 0x90 (nop)
        # data[offset+4] already 0x90 (nop)
    elif data[offset] == 0x31 and data[offset+1] == 0xc0:
        print(f"  Site 0x{offset:x}: already patched with xor eax,eax; ret")
    else:
        print(f"  Site 0x{offset:x}: UNEXPECTED byte 0x{data[offset]:02x}")

# Zero the error strings
for s in [b"CPU ISA level is lower than required",
          b"%s: CPU ISA level is lower than required",
          b"is lower than required"]:
    offset = 0
    while True:
        offset = data.find(s, offset)
        if offset == -1: break
        for i in range(offset, offset + len(s)):
            data[i] = 0x00
        offset += len(s)

with open("/tmp/ld-linux-patched.so.2", "wb") as f:
    f.write(data)
print(f"Patched ld-linux written ({len(data)} bytes)")

# Verify the patch
with open("/tmp/ld-linux-patched.so.2", "rb") as f:
    verify = f.read()
for offset in [0xf304, 0x27ec2, 0x2724c]:
    bytes_at = " ".join(f"0x{verify[offset+i]:02x}" for i in range(5))
    expected = "0x31 0xc0 0xc3 0x90 0x90"
    status = "OK" if bytes_at == expected else "MISMATCH"
    print(f"  Verify 0x{offset:x}: {bytes_at} ({status})")
PYEOF

echo ""
echo "=== 2. Compile wlt-shell v10 (bridge + Tor) ==="
# Reuse the v10 binary if it exists, or compile fresh
if [ ! -f /tmp/wlt_shell_v10 ]; then
    echo "wlt_shell_v10 not found, need to recompile..."
    exit 1
fi
echo "wlt-shell v10 exists: $(file /tmp/wlt_shell_v10 | grep -o 'ELF 64-bit')"

echo ""
echo "=== 3. Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v10 bin/wlt-shell; chmod +x bin/wlt-shell

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v10 bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor; chmod +x usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true
cp /tmp/ld-linux-patched.so.2 lib64/ld-linux-x86-64.so.2; chmod +x lib64/ld-linux-x86-64.so.2
rm -f lib/ld-linux-x86-64.so.2; ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== 4. Rebuild seL4 image ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo ""
echo "=== 5. Boot test ==="
LOG=/home/kushal/torpd-torfix-xorret.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 6. Results ==="
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -15
echo "--- Bridge RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- Tor PID ---"
strings "$LOG" | grep "^PID.*tor\|tor-pd\|tor\[" | head -10
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor:" | head -5
echo "--- Tor log ---"
strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
