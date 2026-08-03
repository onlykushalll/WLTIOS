#!/bin/bash
# Disassemble around the ISA check to find the exact conditional jump pattern.
LD=/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2

echo "=== Disassemble around lea at 0xf2f4 ==="
objdump -d "$LD" 2>/dev/null | grep -A5 -B30 "f2f4:" | head -40

echo ""
echo "=== Disassemble around lea at 0x27eb2 ==="
objdump -d "$LD" 2>/dev/null | grep -A5 -B30 "27eb2:" | head -40

echo ""
echo "=== Try broader approach: NOP the entire check function ==="
# Instead of finding the exact jcc, find the function start (push rbp/endbr64)
# before the lea and replace the entire function with: xor eax,eax; ret
python3 << 'PYEOF'
with open("/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2", "rb") as f:
    data = bytearray(f.read())

error_str = b"CPU ISA level is lower than required"
str_offset = data.find(error_str)

# Find lea references
lea_offsets = []
for i in range(len(data) - 7):
    if data[i] == 0x48 and data[i+1] == 0x8d:
        for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
            if data[i+2] == reg:
                rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
                target = i + 7 + rel
                if target == str_offset:
                    lea_offsets.append(i)
                    print(f"lea at 0x{i:x}")

for lea_off in lea_offsets:
    # Search backwards for function prologue (endbr64 = F3 0F 1E FA, or push rbp = 55)
    for j in range(lea_off - 128, lea_off):
        if j < 0: continue
        # endbr64 pattern
        if data[j] == 0xF3 and data[j+1] == 0x0F and data[j+2] == 0x1E and data[j+3] == 0xFA:
            print(f"Found endbr64 at 0x{j:x} (before lea at 0x{lea_off:x})")
            # Replace with: xor eax, eax (31 C0) + ret (C3) + NOPs
            data[j] = 0x31  # xor
            data[j+1] = 0xC0  # eax, eax
            data[j+2] = 0xC3  # ret
            # NOP the rest up to the lea
            for k in range(j+3, lea_off):
                data[k] = 0x90
            print(f"Patched function at 0x{j:x}: xor eax,eax; ret")
            break
        # push rbp pattern
        if data[j] == 0x55 and data[j+1] == 0x48 and data[j+2] == 0x89 and data[j+3] == 0xE5:
            print(f"Found push rbp at 0x{j:x} (before lea at 0x{lea_off:x})")
            data[j] = 0x31  # xor
            data[j+1] = 0xC0  # eax, eax
            data[j+2] = 0xC3  # ret
            for k in range(j+3, min(j+16, lea_off)):
                data[k] = 0x90
            print(f"Patched function at 0x{j:x}: xor eax,eax; ret")
            break

with open("/tmp/ld-linux-patched.so.2", "wb") as f:
    f.write(data)
print(f"Patched ld-linux written ({len(data)} bytes)")
PYEOF
