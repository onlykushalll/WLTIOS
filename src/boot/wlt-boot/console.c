/**
 * @file console.c
 * @brief UEFI console output implementation
 * @module wlt-boot/console
 * @purpose Print messages during boot via UEFI SimpleTextOut protocol
 * @dependencies: efi.h (UEFI types), console.h
 * @tested_by: tests/boot/console_test.c
 * @spec: docs/specs/wlt-boot.md
 */

#include "console.h"
#include <stdarg.h>

/* UEFI types (minimal — we don't include the full EDK2 headers) */
typedef uint16_t CHAR16;
typedef unsigned long long EFI_STATUS;
typedef void *EFI_HANDLE;

typedef struct {
    void (*Reset)(void *This, int ExtendedVerification);
    void (*OutputString)(void *This, CHAR16 *String);
    void (*TestString)(void *This, CHAR16 *String);
    void (*QueryMode)(void *This, unsigned long ModeNumber,
                      unsigned long *Columns, unsigned long *Rows);
    void (*SetMode)(void *This, unsigned long ModeNumber);
    void (*SetAttribute)(void *This, unsigned long Attribute);
    void (*ClearScreen)(void *This);
    void (*SetCursorPosition)(void *This, unsigned long Column, unsigned long Row);
    void (*EnableCursor)(void *This, int Visible);
    void *Mode;
} SIMPLE_TEXT_OUTPUT_INTERFACE;

typedef struct {
    char                             dummy[56];  /* Header fields we don't use */
    SIMPLE_TEXT_OUTPUT_INTERFACE     *ConOut;
    /* ... more fields after ConOut ... */
} EFI_SYSTEM_TABLE;

/* Module-level state */
static EFI_SYSTEM_TABLE *g_st = NULL;
static SIMPLE_TEXT_OUTPUT_INTERFACE *g_con_out = NULL;

/* UEFI text attributes */
#define EFI_TEXT_ATTR(fg, bg)  ((bg << 4) | fg)
#define EFI_WHITE              0x0F
#define EFI_YELLOW             0x0E
#define EFI_RED                0x0C
#define EFI_LIGHTGRAY          0x07
#define EFI_BLACK              0x00

/* Convert ASCII char to UEFI CHAR16 (wide char) */
static CHAR16
ascii_to_wide(char c)
{
    /* Handle \n → \r\n (UEFI requires carriage return + newline) */
    return (CHAR16)(unsigned char)c;
}

void
wlt_console_init(void *system_table)
{
    g_st = (EFI_SYSTEM_TABLE *)system_table;
    if (g_st != NULL)
    {
        g_con_out = g_st->ConOut;
        if (g_con_out != NULL)
        {
            /* Reset console */
            g_con_out->Reset(g_con_out, 0);
        }
    }
}

void
wlt_console_putc(char c)
{
    if (g_con_out == NULL)
        return;

    /* UEFI requires \r\n for newline, and strings must be CHAR16 */
    CHAR16 str[3];

    if (c == '\n')
    {
        str[0] = (CHAR16)'\r';
        str[1] = (CHAR16)'\n';
        str[2] = 0;
    }
    else
    {
        str[0] = ascii_to_wide(c);
        str[1] = 0;
    }

    g_con_out->OutputString(g_con_out, str);
}

void
wlt_console_puts(const char *str)
{
    if (g_con_out == NULL || str == NULL)
        return;

    /* Convert ASCII string to CHAR16 and output */
    CHAR16 wstr[512];
    int i;

    for (i = 0; str[i] != '\0' && i < 511; i++)
    {
        if (str[i] == '\n')
        {
            /* Expand \n to \r\n */
            wstr[i] = (CHAR16)'\r';
            i++;
            if (i >= 511)
                break;
            wstr[i] = (CHAR16)'\n';
        }
        else
        {
            wstr[i] = ascii_to_wide(str[i]);
        }
    }
    wstr[i] = 0;

    g_con_out->OutputString(g_con_out, wstr);
}

void
wlt_console_printf(const char *fmt, ...)
{
    if (g_con_out == NULL || fmt == NULL)
        return;

    /* Simple printf implementation — supports %s, %d, %x, %u, %c */
    char buf[512];
    int buf_idx = 0;
    va_list args;
    va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0' && buf_idx < 510; i++)
    {
        if (fmt[i] == '%' && fmt[i + 1] != '\0')
        {
            i++;
            switch (fmt[i])
            {
                case 's':
                {
                    const char *s = va_arg(args, const char *);
                    if (s != NULL)
                    {
                        while (*s && buf_idx < 510)
                            buf[buf_idx++] = *s++;
                    }
                    break;
                }
                case 'd':
                {
                    int d = va_arg(args, int);
                    if (d < 0)
                    {
                        buf[buf_idx++] = '-';
                        d = -d;
                    }
                    /* Convert to string (reverse) */
                    char tmp[16];
                    int len = 0;
                    if (d == 0)
                        tmp[len++] = '0';
                    while (d > 0 && len < 15)
                    {
                        tmp[len++] = '0' + (d % 10);
                        d /= 10;
                    }
                    while (len > 0 && buf_idx < 510)
                        buf[buf_idx++] = tmp[--len];
                    break;
                }
                case 'x':
                {
                    unsigned int x = va_arg(args, unsigned int);
                    char tmp[16];
                    int len = 0;
                    if (x == 0)
                        tmp[len++] = '0';
                    while (x > 0 && len < 15)
                    {
                        int digit = x & 0xF;
                        tmp[len++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
                        x >>= 4;
                    }
                    while (len > 0 && buf_idx < 510)
                        buf[buf_idx++] = tmp[--len];
                    break;
                }
                case 'u':
                {
                    unsigned int u = va_arg(args, unsigned int);
                    char tmp[16];
                    int len = 0;
                    if (u == 0)
                        tmp[len++] = '0';
                    while (u > 0 && len < 15)
                    {
                        tmp[len++] = '0' + (u % 10);
                        u /= 10;
                    }
                    while (len > 0 && buf_idx < 510)
                        buf[buf_idx++] = tmp[--len];
                    break;
                }
                case 'c':
                {
                    char c = (char)va_arg(args, int);
                    buf[buf_idx++] = c;
                    break;
                }
                case '%':
                    buf[buf_idx++] = '%';
                    break;
                default:
                    buf[buf_idx++] = '%';
                    buf[buf_idx++] = fmt[i];
                    break;
            }
        }
        else
        {
            buf[buf_idx++] = fmt[i];
        }
    }

    buf[buf_idx] = '\0';
    va_end(args);

    wlt_console_puts(buf);
}

void
wlt_console_info(const char *msg)
{
    if (g_con_out != NULL)
        g_con_out->SetAttribute(g_con_out, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
    wlt_console_puts("[INFO] ");
    wlt_console_puts(msg);
    wlt_console_puts("\r\n");
}

void
wlt_console_warn(const char *msg)
{
    if (g_con_out != NULL)
        g_con_out->SetAttribute(g_con_out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
    wlt_console_puts("[WARN] ");
    wlt_console_puts(msg);
    wlt_console_puts("\r\n");
    if (g_con_out != NULL)
        g_con_out->SetAttribute(g_con_out, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

void
wlt_console_error(const char *msg)
{
    if (g_con_out != NULL)
        g_con_out->SetAttribute(g_con_out, EFI_TEXT_ATTR(EFI_RED, EFI_BLACK));
    wlt_console_puts("[ERROR] ");
    wlt_console_puts(msg);
    wlt_console_puts("\r\n");
    if (g_con_out != NULL)
        g_con_out->SetAttribute(g_con_out, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

void
wlt_console_clear(void)
{
    if (g_con_out != NULL)
    {
        g_con_out->ClearScreen(g_con_out);
    }
}
