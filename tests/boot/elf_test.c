/**
 * @file elf_test.c
 * @brief Unit tests for WLT-Boot ELF parser
 * @module wlt-boot/loader
 * @purpose Test ELF64 header verification
 * @spec: docs/specs/wlt-boot.md
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../src/boot/wlt-boot/include/loader.h"

/* Mock console functions */
void wlt_console_init(void *st) {}
void wlt_console_puts(const char *s) { (void)s; }
void wlt_console_printf(const char *fmt, ...) { (void)fmt; }
void wlt_console_info(const char *msg) { (void)msg; }
void wlt_console_warn(const char *msg) { (void)msg; }
void wlt_console_error(const char *msg) { (void)msg; }
void wlt_console_clear(void) {}
void wlt_console_putc(char c) { (void)c; }
/* wlt_loader_set_system_table is in loader.c — don't mock it */

/* ELF constants */
#define ELFMAG0    0x7f
#define ELFMAG1    'E'
#define ELFMAG2    'L'
#define ELFMAG3    'F'
#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define ET_EXEC    2
#define EM_X86_64  62

/* Build a minimal valid ELF64 header */
static void
build_valid_elf64(uint8_t *buf, size_t size)
{
    memset(buf, 0, size);

    /* e_ident */
    buf[0] = ELFMAG0;
    buf[1] = ELFMAG1;
    buf[2] = ELFMAG2;
    buf[3] = ELFMAG3;
    buf[4] = ELFCLASS64;      /* 64-bit */
    buf[5] = ELFDATA2LSB;     /* Little endian */
    buf[6] = 1;                /* ELF version */
    buf[7] = 0;                /* ELFOSABI_NONE */

    /* e_type (offset 16) = ET_EXEC */
    *(uint16_t *)(buf + 16) = ET_EXEC;

    /* e_machine (offset 18) = EM_X86_64 */
    *(uint16_t *)(buf + 18) = EM_X86_64;

    /* e_version (offset 20) = 1 */
    *(uint32_t *)(buf + 20) = 1;

    /* e_entry (offset 24) = 0x100000 */
    *(uint64_t *)(buf + 24) = 0x100000;

    /* e_phoff (offset 32) = 64 (right after header) */
    *(uint64_t *)(buf + 32) = 64;

    /* e_phentsize (offset 54) = 56 (sizeof elf64_phdr) */
    *(uint16_t *)(buf + 54) = 56;

    /* e_phnum (offset 56) = 1 */
    *(uint16_t *)(buf + 56) = 1;

    /* e_ehsize (offset 52) = 64 */
    *(uint16_t *)(buf + 52) = 64;
}

int main(void)
{
    int pass = 0, fail = 0;
    uint8_t buf[256];

    printf("=== WLT-Boot ELF Parser Test ===\n\n");

    /* Test 1: Valid ELF64 */
    build_valid_elf64(buf, sizeof(buf));
    if (wlt_loader_verify_elf(buf, sizeof(buf)) == WLT_BOOT_OK)
    {
        printf("[PASS] Test 1: Valid ELF64 accepted\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 1: Valid ELF64 rejected\n");
        fail++;
    }

    /* Test 2: Invalid magic */
    build_valid_elf64(buf, sizeof(buf));
    buf[0] = 0x00;  /* Bad magic */
    if (wlt_loader_verify_elf(buf, sizeof(buf)) != WLT_BOOT_OK)
    {
        printf("[PASS] Test 2: Invalid magic rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 2: Invalid magic should be rejected\n");
        fail++;
    }

    /* Test 3: 32-bit ELF rejected (we want 64-bit) */
    build_valid_elf64(buf, sizeof(buf));
    buf[4] = 1;  /* ELFCLASS32 */
    if (wlt_loader_verify_elf(buf, sizeof(buf)) != WLT_BOOT_OK)
    {
        printf("[PASS] Test 3: 32-bit ELF rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 3: 32-bit should be rejected\n");
        fail++;
    }

    /* Test 4: Wrong machine type */
    build_valid_elf64(buf, sizeof(buf));
    *(uint16_t *)(buf + 18) = 3;  /* EM_386 instead of EM_X86_64 */
    if (wlt_loader_verify_elf(buf, sizeof(buf)) != WLT_BOOT_OK)
    {
        printf("[PASS] Test 4: Wrong machine rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 4: Wrong machine should be rejected\n");
        fail++;
    }

    /* Test 5: Get entry point */
    build_valid_elf64(buf, sizeof(buf));
    uint64_t entry = wlt_loader_get_entry(buf);
    if (entry == 0x100000)
    {
        printf("[PASS] Test 5: Entry point = 0x%llx\n",
               (unsigned long long)entry);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 5: Expected 0x100000, got 0x%llx\n",
               (unsigned long long)entry);
        fail++;
    }

    /* Test 6: NULL data rejected */
    if (wlt_loader_verify_elf(NULL, 100) != WLT_BOOT_OK)
    {
        printf("[PASS] Test 6: NULL data rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 6: NULL should be rejected\n");
        fail++;
    }

    /* Test 7: Too small data rejected */
    if (wlt_loader_verify_elf(buf, 10) != WLT_BOOT_OK)
    {
        printf("[PASS] Test 7: Too-small data rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 7: Small data should be rejected\n");
        fail++;
    }

    /* Test 8: Non-executable type rejected */
    build_valid_elf64(buf, sizeof(buf));
    *(uint16_t *)(buf + 16) = 4;  /* ET_CORE instead of ET_EXEC */
    if (wlt_loader_verify_elf(buf, sizeof(buf)) != WLT_BOOT_OK)
    {
        printf("[PASS] Test 8: Non-executable rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 8: Core file should be rejected\n");
        fail++;
    }

    printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
