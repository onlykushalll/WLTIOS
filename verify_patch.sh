#!/bin/bash
# Verify the patched ld-linux is in the staged rootfs.
echo "=== Check staged rootfs_vm1.cpio for patched ld-linux ==="
cd /tmp && rm -rf verify-patched && mkdir verify-patched && cd verify-patched
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio 2>/dev/null

echo "--- ld-linux file ---"
file lib64/ld-linux-x86-64.so.2
ls -la lib64/ld-linux-x86-64.so.2

echo ""
echo "--- Check if ISA string is still in ld-linux ---"
strings lib64/ld-linux-x86-64.so.2 | grep "CPU ISA\|ISA level" | head -3

echo ""
echo "--- Check if the conditional jumps were NOPed ---"
# The original has jcc near (0F 8X) before the lea. Check if those are now 90 (NOP).
python3 << 'PYEOF'
with open("lib64/ld-linux-x86-64.so.2", "rb") as f:
    data = f.read()

error_str = b"CPU ISA level is lower than required"
str_offset = data.find(error_str)
print(f"Error string at: 0x{str_offset:x}")

# Find lea references
for i in range(len(data) - 7):
    if data[i] == 0x48 and data[i+1] == 0x8d:
        for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
            if data[i+2] == reg:
                rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
                target = i + 7 + rel
                if target == str_offset:
                    print(f"lea at 0x{i:x} -> string at 0x{target:x}")
                    # Check 32 bytes before for jcc or NOP
                    for j in range(i - 32, i - 2):
                        if j < 0: continue
                        b = data[j]
                        if b == 0x0F and j+1 < len(data) and 0x80 <= data[j+1] <= 0x8F:
                            # Check if it's NOPed
                            if data[j] == 0x90:
                                print(f"  NOPed at 0x{j:x}")
                            else:
                                print(f"  UNPATCHED jcc at 0x{j:x} (0x{b:02x} 0x{data[j+1]:02x})")
                            break
                        elif 0x70 <= b <= 0x7F:
                            if data[j] == 0x90:
                                print(f"  NOPed at 0x{j:x}")
                            else:
                                print(f"  UNPATCHED jcc short at 0x{j:x} (0x{b:02x})")
                            break
PYEOF

echo ""
echo "--- tor binary permissions ---"
ls -la usr/bin/tor
echo "--- tor binary executable? ---"
test -x usr/bin/tor && echo "YES - executable" || echo "NO - not executable"
