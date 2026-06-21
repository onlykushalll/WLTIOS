/**
 * @file console.h
 * @brief UEFI console output for WLT-Boot
 * @module wlt-boot/console
 * @purpose Print messages to UEFI text console during boot
 * @dependencies: efi.h
 * @spec: docs/specs/wlt-boot.md
 */

#ifndef WLT_CONSOLE_H
#define WLT_CONSOLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize console output
 */
void wlt_console_init(void *system_table);

/**
 * Print a character
 */
void wlt_console_putc(char c);

/**
 * Print a string
 */
void wlt_console_puts(const char *str);

/**
 * Print formatted (like printf, limited)
 */
void wlt_console_printf(const char *fmt, ...);

/**
 * Print with log level
 */
void wlt_console_info(const char *msg);
void wlt_console_warn(const char *msg);
void wlt_console_error(const char *msg);

/**
 * Clear screen
 */
void wlt_console_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* WLT_CONSOLE_H */
