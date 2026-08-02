#!/bin/bash
# Fetch real UEFI type definitions from gnu-efi (canonical open-source UEFI SDK).
set -e
WORK=/tmp/uefi-defs
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"

# gnu-efi is the standard. Fetch the key headers from the official repo.
echo "=== Fetching gnu-efi headers ==="
for f in efidef.h efitypes.h efiapi.h eficon.h efiboot.h efiprot.h; do
  curl -sSL "https://raw.githubusercontent.com/gnu-efi/gnu-efi/master/inc/$f" -o "$f" 2>/dev/null
  echo "  $f: $(wc -l < $f 2>/dev/null || echo FAIL) lines"
done

echo ""
echo "=== EFI_SYSTEM_TABLE definition ==="
grep -A30 "typedef struct.*EFI_SYSTEM_TABLE" efitypes.h 2>/dev/null | head -35

echo ""
echo "=== EFI_BOOT_SERVICES definition ==="
grep -A80 "typedef struct.*EFI_BOOT_SERVICES" efiapi.h 2>/dev/null | head -90

echo ""
echo "=== GetMemoryMap signature ==="
grep -B2 -A5 "GetMemoryMap" efiapi.h 2>/dev/null | head -20

echo ""
echo "=== ExitBootServices signature ==="
grep -B2 -A5 "ExitBootServices" efiapi.h 2>/dev/null | head -15
