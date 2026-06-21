/**
 * @file linker.ld
 * @brief UEFI linker script for WLT-Boot
 * @module wlt-boot
 * @purpose Defines memory layout for UEFI bootloader application
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md
 *
 * UEFI applications are PE/COFF format, but we link as ELF first
 * then convert to PE via objcopy. This script defines the sections.
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    /* UEFI applications are loaded at a high address */
    . = 0x100000;

    .text :
    {
        *(.text.efi_main)     /* Entry point first */
        *(.text*)
        *(.rodata*)
    }

    .data :
    {
        *(.data*)
        *(.bss*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.eh_frame_hdr*)
    }
}
