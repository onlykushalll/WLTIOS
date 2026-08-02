#!/bin/bash
echo "=== EFI_SYSTEM_TABLE ==="
grep -A30 'typedef struct _EFI_SYSTEM_TABLE' /usr/include/efi/efiapi.h
echo ""
echo "=== EFI_BOOT_SERVICES (first 90 lines) ==="
grep -A90 'typedef struct _EFI_BOOT_SERVICES' /usr/include/efi/efiapi.h
echo ""
echo "=== EFI_TABLE_HEADER ==="
grep -A8 'typedef struct.*EFI_TABLE_HEADER' /usr/include/efi/efiapi.h /usr/include/efi/efidef.h 2>/dev/null | head -10
echo ""
echo "=== GetMemoryMap typedef ==="
grep -B1 -A3 'GetMemoryMap' /usr/include/efi/efiapi.h | head -10
echo ""
echo "=== ExitBootServices typedef ==="
grep -B1 -A3 'ExitBootServices' /usr/include/efi/efiapi.h | head -10
echo ""
echo "=== AllocatePages typedef ==="
grep -B1 -A3 'AllocatePages' /usr/include/efi/efiapi.h | head -10
echo ""
echo "=== AllocatePool typedef ==="
grep -B1 -A3 'AllocatePool' /usr/include/efi/efiapi.h | head -10
