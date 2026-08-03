#!/bin/bash
# Find ALL references to the ISA error string and patch every call after them.
set -e

python3 << 'PYEOF'
ld_path = "/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2"
with open(ld_path, "rb") as f:
    data = bytearray(f.read())

# Find ALL error string variants
strings_to_find = [
    b"CPU ISA level is lower than required",
    b"is lower than required",
    b"%s: CPU ISA level is lower than required",
]

patched = 0
for error_str in strings_to_find:
    str_offset = data.find(error_str)
    if str_offset == -1:
        continue
    print(f"\nSearching for references to '{error_str.decode()[:40]}' at 0x{str_offset:x}")
    
    # Find ALL lea instructions that reference this string
    for i in range(len(data) - 7):
        if data[i] == 0x48 and data[i+1] == 0x8d:
            for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
                if data[i+2] == reg:
                    rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
                    target = i + 7 + rel
                    if target == str_offset:
                        print(f"  Found lea at 0x{i:x}")
                        # Search FORWARD for the next call instruction (e8 XX XX XX XX)
                        for j in range(i, min(i + 48, len(data) - 5)):
                            if data[j] == 0xe8:
                                print(f"    Found call at 0x{j:x} -> patching to ret")
                                data[j] = 0xc3  # ret
                                for k in range(j+1, j+5):
                                    data[k] = 0x90  # nop
                                patched += 1
                                break

# Also find ALL calls to _dl_signal_error (at offset 0x2240 in the binary)
# by searching for e8 calls with the right relative offset
print("\nSearching for ALL calls to _dl_signal_error...")
for i in range(len(data) - 5):
    if data[i] == 0xe8:
        rel = int.from_bytes(data[i+1:i+5], 'little', signed=True)
        target = i + 5 + rel
        if target == 0x2240:  # _dl_signal_error address
            # Check if this call is near an ISA error string reference
            # Look 48 bytes before for a lea that references an ISA string
            near_isa = False
            for j in range(max(0, i-48), i):
                if data[j] == 0x48 and data[j+1] == 0x8d:
                    for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
                        if data[j+2] == reg:
                            rel2 = int.from_bytes(data[j+3:j+7], 'little', signed=True)
                            target2 = j + 7 + rel2
                            for s in strings_to_find:
                                if target2 == data.find(s):
                                    near_isa = True
                                    break
            if near_isa:
                print(f"  Patching call at 0x{i:x} (near ISA string ref)")
                data[i] = 0xc3
                for k in range(i+1, i+5):
                    data[k] = 0x90
                patched += 1

# Nuclear option: NOP out the ISA check function entirely.
# Find _dl_check_isa_level by looking for the function that contains BOTH lea references.
# The function likely starts with endbr64 (F3 0F 1E FA) or push rbp (55 48 89 E5).
# Replace the entire function with xor eax,eax; ret (returns 0 = success).

# Also: just overwrite the error string itself to make it empty
for error_str in strings_to_find:
    offset = data.find(error_str)
    if offset != -1:
        # Replace with null bytes
        for i in range(offset, offset + len(error_str)):
            data[i] = 0x00
        print(f"Zeroed string at 0x{offset:x}")

print(f"\nTotal patches: {patched}")
with open("/tmp/ld-linux-patched.so.2", "wb") as f:
    f.write(data)
print(f"Patched ld-linux written ({len(data)} bytes)")
PYEOF

echo ""
echo "=== Verify patch ==="
python3 -c "
with open('/tmp/ld-linux-patched.so.2', 'rb') as f:
    data = f.read()
# Check if error string is still present
if b'CPU ISA level' in data:
    print('WARNING: Error string still in binary')
else:
    print('Error string removed')
# Check patched call sites
for offset in [0xf304, 0x27ec2]:
    print(f'Byte at 0x{offset:x}: 0x{data[offset]:02x} (should be 0xc3 = ret)')
"
