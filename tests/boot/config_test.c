/**
 * @file config_test.c
 * @brief Unit tests for WLT-Boot config parser
 * @module wlt-boot/config
 * @purpose Test INI parsing for boot config
 * @spec: docs/specs/wlt-boot.md
 */

#include <stdio.h>
#include <string.h>
#include "../../src/boot/wlt-boot/include/boot.h"
#include "../../src/boot/wlt-boot/include/config.h"

/* Mock console functions (config.c calls them) */
void wlt_console_init(void *st) {}
void wlt_console_puts(const char *s) { (void)s; }
void wlt_console_printf(const char *fmt, ...) { (void)fmt; }
void wlt_console_info(const char *msg) { (void)msg; }
void wlt_console_warn(const char *msg) { (void)msg; }
void wlt_console_error(const char *msg) { (void)msg; }
void wlt_console_clear(void) {}
void wlt_console_putc(char c) { (void)c; }

int main(void)
{
    int pass = 0, fail = 0;

    printf("=== WLT-Boot Config Test ===\n\n");

    /* Test 1: Default config */
    wlt_boot_config_t config;
    wlt_config_get_default(&config);

    if (strcmp(config.kernel_path, "\\boot\\sel4.bin") == 0)
    {
        printf("[PASS] Test 1: Default kernel path = %s\n", config.kernel_path);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 1: Expected \\boot\\sel4.bin, got %s\n", config.kernel_path);
        fail++;
    }

    /* Test 2: Default signature path */
    if (strcmp(config.signature_path, "\\boot\\sel4.sig") == 0)
    {
        printf("[PASS] Test 2: Default signature path = %s\n", config.signature_path);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 2: Expected \\boot\\sel4.sig, got %s\n", config.signature_path);
        fail++;
    }

    /* Test 3: Default verify_signature = true */
    if (config.verify_signature == true)
    {
        printf("[PASS] Test 3: Default verify_signature = true\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 3: Expected true, got false\n");
        fail++;
    }

    /* Test 4: Default timeout */
    if (config.timeout_ms == WLT_BOOT_TIMEOUT_MS)
    {
        printf("[PASS] Test 4: Default timeout = %u ms\n", config.timeout_ms);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 4: Expected %u, got %u\n", WLT_BOOT_TIMEOUT_MS, config.timeout_ms);
        fail++;
    }

    /* Test 5: Validate valid config */
    wlt_boot_error_t err = wlt_config_validate(&config);
    if (err == WLT_BOOT_OK)
    {
        printf("[PASS] Test 5: Valid config passes validation\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 5: Valid config failed validation: %d\n", err);
        fail++;
    }

    /* Test 6: Invalid config (empty kernel path) */
    wlt_boot_config_t bad_config;
    wlt_config_get_default(&bad_config);
    bad_config.kernel_path[0] = '\0';
    err = wlt_config_validate(&bad_config);
    if (err != WLT_BOOT_OK)
    {
        printf("[PASS] Test 6: Empty kernel path rejected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 6: Empty kernel path should be rejected\n");
        fail++;
    }

    printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
