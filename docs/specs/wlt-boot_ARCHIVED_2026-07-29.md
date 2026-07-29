# Spec: WLT-Boot (Bootloader)

> **Status:** Draft (ready for implementation)
> **Phase:** 1 (Month 1)
> **Dependencies:** UEFI firmware, seL4 kernel image
> **ADR references:** ADR-01 (UEFI), ADR-02 (seL4 base)

## Purpose

WLT-Boot is the UEFI bootloader for WLTIOS. It is the first piece of code that runs after the UEFI firmware. Its job is to load seL4 into memory, verify its signature, and transfer control to it.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| seL4 kernel image | EFI System Partition (ESP) | ELF binary |
| seL4 signature | ESP | SHA-256 signature |
| Memory map | UEFI firmware | EFI_MEMORY_DESCRIPTOR array |
| Boot configuration | ESP (`/boot/wltios.conf`) | INI-style text |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| seL4 in memory | RAM (physical address) | Loaded ELF |
| Memory map | seL4 (via bootinfo) | EFI_MEMORY_DESCRIPTOR array |
| Boot info struct | seL4 (via register) | wlt_boot_info_t |
| Control transfer | seL4 entry point | Jump to seL4 |

## Dependencies

- **UEFI firmware** (provides boot services, memory map, file access)
- **seL4 kernel image** (must be built and signed before boot)
- **No other dependencies** (WLT-Boot is self-contained)

## What Depends on This

- seL4 (cannot boot without WLT-Boot)
- Everything (WLT-Boot is the root of the boot chain)

## Error Handling

| Error | Action |
|-------|--------|
| seL4 image not found | Print error, drop to UEFI shell |
| Signature verification failed | Print warning, refuse to boot (unless user overrides) |
| Memory map retrieval failed | Print error, halt |
| seL4 too large for available RAM | Print error, halt |
| Boot config parse error | Use defaults, print warning |

## Tests

| Test | How |
|------|-----|
| Boot in QEMU with OVMF | Boot WLT-Boot in QEMU, verify seL4 starts |
| Signature verification (valid) | Boot with valid signature, verify success |
| Signature verification (invalid) | Boot with invalid signature, verify refusal |
| Memory map correctness | Boot, dump memory map, verify against QEMU config |
| Large seL4 image | Test with seL4 image > available RAM |
| Missing config file | Boot without wltios.conf, verify defaults used |
| Corrupt config file | Boot with malformed config, verify defaults used |

## Security

- **Verified boot:** WLT-Boot checks seL4's SHA-256 signature before loading. If signature is invalid, boot is refused.
- **No network access:** WLT-Boot cannot download updates. All images must be on disk.
- **No user input (except override):** WLT-Boot doesn't accept commands. Only a physical "override" key (e.g., specific key held during boot) bypasses signature check.
- **Secure Boot integration:** WLT-Boot itself is signed for UEFI Secure Boot.

## Performance

| Metric | Budget |
|--------|--------|
| Time to load seL4 | < 100ms |
| Time to verify signature | < 50ms |
| Total boot time (WLT-Boot) | < 500ms |
| WLT-Boot binary size | < 64KB |

## Implementation Notes

- **Language:** C (with inline assembly for UEFI calls)
- **UEFI standard:** UEFI 2.8+ (x86-64)
- **Build:** Cross-compiled with GCC (x86_64-elf-gcc) or clang
- **Linker:** Custom linker script for UEFI application
- **No external libraries:** Only UEFI firmware APIs

## File Structure

```
src/boot/wlt-boot/
├── include/
│   ├── boot.h          # Public boot interface
│   ├── config.h        # Boot config parsing
│   ├── memory.h        # Memory map handling
│   ├── verify.h        # Signature verification
│   └── efi.h           # UEFI type wrappers
├── boot.c              # Main entry point (efi_main)
├── memory.c            # Memory map retrieval + filtering
├── loader.c            # seL4 ELF loading
├── verify.c            # SHA-256 signature verification
├── config.c            # Boot config file parsing
├── console.c           # UEFI console output
└── linker.ld           # Linker script for UEFI app
```

## Boot Flow

```
1. UEFI firmware loads WLT-Boot from ESP
2. WLT-Boot initializes console (print "WLTIOS Boot Loader v1.0")
3. WLT-Boot retrieves UEFI memory map
4. WLT-Boot reads boot config from /boot/wltios.conf
5. WLT-Boot loads seL4 ELF from /boot/sel4.bin
6. WLT-Boot reads seL4 signature from /boot/sel4.sig
7. WLT-Boot verifies signature (SHA-256)
   - If invalid: print error, halt (unless override key held)
8. WLT-Boot parses seL4 ELF, loads segments into memory
9. WLT-Boot prepares boot info struct (memory map, config)
10. WLT-Boot calls seL4 entry point with boot info
11. seL4 takes over (WLT-Boot's job is done)
```

## Boot Config Format (`/boot/wltios.conf`)

```ini
[wlt-boot]
version = 1
timeout = 3                    # Seconds to wait before auto-boot
default = sel4                 # Default boot entry

[sel4]
kernel = /boot/sel4.bin        # seL4 kernel image
signature = /boot/sel4.sig     # Signature file
verify = yes                   # Verify signature before boot
cmdline = "console=serial"     # Command line passed to seL4

[recovery]
kernel = /boot/sel4-recovery.bin
signature = /boot/sel4-recovery.sig
verify = yes
cmdline = "recovery=1"
```

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after seL4 build is working)
