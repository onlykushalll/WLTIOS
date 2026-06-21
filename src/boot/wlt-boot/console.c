/**
 * @file console.c
 * @brief UEFI console output implementation
 * @module wlt-boot/console
 * @purpose Print messages during boot
 * @spec: docs/specs/wlt-boot.md
 */

#include "console.h"

/* TODO(wlt-boot): Implement UEFI console output */
/* This requires UEFI protocols: */
/*   - gEfiSimpleTextOutProtocolGuid */
/*   - SystemTable->ConOut->OutputString */

static void *g_system_table = NULL;

void
wlt_console_init(void *system_table)
{
    g_system_table = system_table;
}

void
wlt_console_putc(char c)
{
    /* TODO(wlt-boot): Output character via UEFI ConOut */
    (void)c;
}

void
wlt_console_puts(const char *str)
{
    /* TODO(wlt-boot): Output string via UEFI ConOut */
    (void)str;
}

void
wlt_console_printf(const char *fmt, ...)
{
    /* TODO(wlt-boot): Implement printf-style formatting */
    (void)fmt;
}

void
wlt_console_info(const char *msg)
{
    wlt_console_puts("[INFO] ");
    wlt_console_puts(msg);
    wlt_console_puts("\r\n");
}

void
wlt_console_warn(const char *msg)
{
    wlt_console_puts("[WARN] ");
    wlt_console_puts(msg);
    wlt_console_puts("\r\n");
}

void
wlt_console_error(const char *msg)
{
    wlt_console_puts("[ERROR] ");
    wlt_console_puts(msg);
    wlt_console_puts("\r\n");
}

void
wlt_console_clear(void)
{
    /* TODO(wlt-boot): Clear screen via UEFI ConOut->ClearScreen */
}
