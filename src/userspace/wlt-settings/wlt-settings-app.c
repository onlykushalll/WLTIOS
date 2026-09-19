/**
 * @file wlt-settings-app.c
 * @brief WLTIOS Settings UI -- Wayland client + headless self-test
 * @module wlt-settings
 *
 * PHASE DESKTOP-SHELL rebuild (2026-09-19). This file previously had:
 *   - font5x7[96][7] = {0}  (every glyph all-zero) and a renderer that
 *     drew a uniform 6x10 COLOR BLOCK per character -- all text rendered
 *     as colored squares, unreadable.
 *   - No wl_seat binding (registry bound only compositor/shm/xdg_wm_base).
 *   - Zero connection to the settings engine this module ships: the app
 *     never instantiated wlt_settings_t, never registered the four panels,
 *     never loaded persisted values.
 *
 * Now:
 *   - Real 5x7 bitmap glyphs (public-domain classic table), 6x8 cells,
 *     colored text on navy background.
 *   - wl_seat v1 bound; pointer + keyboard tracked; live pointer coords
 *     and keyboard state in the status bar.
 *   - The settings engine is instantiated: all four panels (display,
 *     network, privacy, security) register their schema, persisted values
 *     are loaded via config I/O, and every panel section in the window
 *     shows its persisted key values. Live enactment still reports
 *     honestly-UNREACHABLE (wlt_display_apply): compositor IPC does not
 *     exist yet.
 *   - `--selftest` renders the SAME grid through the SAME glyph renderer
 *     into an anonymous buffer (no Wayland), writes a PPM dump, and
 *     verifies text rows show glyph-like pixel variance (an all-zero font
 *     or block renderer cannot pass). Exit 0 = PASS.
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "settings.h"

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"

/* ------------------------------------------------------------------ */
/* Window metrics                                                      */
/* ------------------------------------------------------------------ */
#define WIN_W 700
#define WIN_H 500
#define GLYPH_W 5
#define GLYPH_H 7
#define CELL_W 6          /* 5 px glyph + 1 px spacing */
#define CELL_H 8          /* 7 px glyph + 1 px spacing */
#define MARGIN_X 8
#define MARGIN_Y 6
#define COLS ((WIN_W - 2 * MARGIN_X) / CELL_W)   /* 114 */
#define ROWS ((WIN_H - 2 * MARGIN_Y) / CELL_H)   /* 61  */

/* Terminal grid: rows of attribute+char cells */
typedef struct { char ch; uint32_t fg; } cell_t;
static cell_t grid[ROWS][COLS];
static int cursor_row = 0, cursor_col = 0;

#define COL_BG      0xFF0A1628u   /* navy */
#define COL_HDR     0xFFFFFFFFu   /* white */
#define COL_SECTION 0xFF55DDFFu   /* light blue */
#define COL_TEXT    0xFF00E88Au   /* green */
#define COL_VALUE   0xFFFFD24Au   /* amber */
#define COL_DIM     0xFF8899AAu   /* grey */
#define COL_WARN    0xFFFF9944u   /* orange */
#define COL_INPUT   0xFFFFFF66u   /* pale yellow */

/* ------------------------------------------------------------------ */
/* Real 5x7 bitmap font, ASCII 0x20..0x7F (96 glyphs).                 */
/* Row-major: 7 bytes per glyph, one byte per row, bit 4 = leftmost    */
/* of the 5 pixel columns (0x10 .. 0x01). Public-domain classic.       */
/* Replaces the all-zero placeholder -- see file header.               */
/* ------------------------------------------------------------------ */
static const uint8_t font5x7[96][7] = {
    /* 0x20 ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x21 '!' */ {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    /* 0x22 '"' */ {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
    /* 0x23 '#' */ {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A},
    /* 0x24 '$' */ {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
    /* 0x25 '%' */ {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
    /* 0x26 '&' */ {0x0E,0x11,0x10,0x08,0x15,0x12,0x0D},
    /* 0x27 '\'*/ {0x04,0x04,0x00,0x00,0x00,0x00,0x00},
    /* 0x28 '(' */ {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    /* 0x29 ')' */ {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    /* 0x2A '*' */ {0x00,0x04,0x15,0x0E,0x15,0x04,0x00},
    /* 0x2B '+' */ {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    /* 0x2C ',' */ {0x00,0x00,0x00,0x00,0x04,0x04,0x08},
    /* 0x2D '-' */ {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    /* 0x2E '.' */ {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
    /* 0x2F '/' */ {0x00,0x01,0x02,0x04,0x08,0x10,0x00},
    /* 0x30 '0' */ {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    /* 0x31 '1' */ {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    /* 0x32 '2' */ {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    /* 0x33 '3' */ {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    /* 0x34 '4' */ {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    /* 0x35 '5' */ {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    /* 0x36 '6' */ {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    /* 0x37 '7' */ {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    /* 0x38 '8' */ {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    /* 0x39 '9' */ {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    /* 0x3A ':' */ {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},
    /* 0x3B ';' */ {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08},
    /* 0x3C '<' */ {0x02,0x04,0x08,0x10,0x08,0x04,0x02},
    /* 0x3D '=' */ {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
    /* 0x3E '>' */ {0x08,0x04,0x02,0x01,0x02,0x04,0x08},
    /* 0x3F '?' */ {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},
    /* 0x40 '@' */ {0x0E,0x11,0x15,0x17,0x16,0x10,0x0E},
    /* 0x41 'A' */ {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    /* 0x42 'B' */ {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    /* 0x43 'C' */ {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    /* 0x44 'D' */ {0x1C,0x11,0x11,0x11,0x11,0x11,0x1C},
    /* 0x45 'E' */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    /* 0x46 'F' */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    /* 0x47 'G' */ {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    /* 0x48 'H' */ {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    /* 0x49 'I' */ {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    /* 0x4A 'J' */ {0x01,0x01,0x01,0x01,0x01,0x11,0x0E},
    /* 0x4B 'K' */ {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    /* 0x4C 'L' */ {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    /* 0x4D 'M' */ {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    /* 0x4E 'N' */ {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    /* 0x4F 'O' */ {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    /* 0x50 'P' */ {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    /* 0x51 'Q' */ {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    /* 0x52 'R' */ {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    /* 0x53 'S' */ {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    /* 0x54 'T' */ {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    /* 0x55 'U' */ {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    /* 0x56 'V' */ {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    /* 0x57 'W' */ {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    /* 0x58 'X' */ {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    /* 0x59 'Y' */ {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    /* 0x5A 'Z' */ {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    /* 0x5B '[' */ {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
    /* 0x5C '\' */ {0x00,0x10,0x08,0x04,0x02,0x01,0x00},
    /* 0x5D ']' */ {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
    /* 0x5E '^' */ {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},
    /* 0x5F '_' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
    /* 0x60 '`' */ {0x08,0x04,0x00,0x00,0x00,0x00,0x00},
    /* 0x61 'a' */ {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
    /* 0x62 'b' */ {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
    /* 0x63 'c' */ {0x00,0x00,0x0F,0x10,0x10,0x10,0x0F},
    /* 0x64 'd' */ {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F},
    /* 0x65 'e' */ {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
    /* 0x66 'f' */ {0x06,0x08,0x08,0x1E,0x08,0x08,0x08},
    /* 0x67 'g' */ {0x00,0x0E,0x11,0x11,0x0F,0x01,0x1E},
    /* 0x68 'h' */ {0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
    /* 0x69 'i' */ {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
    /* 0x6A 'j' */ {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
    /* 0x6B 'k' */ {0x10,0x10,0x12,0x14,0x18,0x14,0x12},
    /* 0x6C 'l' */ {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
    /* 0x6D 'm' */ {0x00,0x00,0x1A,0x15,0x15,0x15,0x15},
    /* 0x6E 'n' */ {0x00,0x00,0x1E,0x11,0x11,0x11,0x11},
    /* 0x6F 'o' */ {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
    /* 0x70 'p' */ {0x00,0x1E,0x11,0x11,0x1E,0x10,0x10},
    /* 0x71 'q' */ {0x00,0x0F,0x11,0x11,0x0F,0x01,0x01},
    /* 0x72 'r' */ {0x00,0x00,0x1C,0x12,0x10,0x10,0x10},
    /* 0x73 's' */ {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},
    /* 0x74 't' */ {0x08,0x08,0x1E,0x08,0x08,0x09,0x06},
    /* 0x75 'u' */ {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},
    /* 0x76 'v' */ {0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
    /* 0x77 'w' */ {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},
    /* 0x78 'x' */ {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
    /* 0x79 'y' */ {0x00,0x00,0x11,0x11,0x0F,0x01,0x1E},
    /* 0x7A 'z' */ {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},
    /* 0x7B '{' */ {0x06,0x08,0x08,0x10,0x08,0x08,0x06},
    /* 0x7C '|' */ {0x04,0x04,0x04,0x04,0x04,0x04,0x04},
    /* 0x7D '}' */ {0x0C,0x02,0x02,0x01,0x02,0x02,0x0C},
    /* 0x7E '~' */ {0x00,0x00,0x08,0x15,0x02,0x00,0x00},
};

/* ------------------------------------------------------------------ */
/* Terminal grid helpers                                               */
/* ------------------------------------------------------------------ */
static uint32_t cur_color = COL_TEXT;

static void term_color(uint32_t c) { cur_color = c; }

static void term_print(const char *s)
{
    while (*s && cursor_row < ROWS) {
        if (*s == '\n') {
            cursor_col = 0;
            cursor_row++;
        } else {
            if (cursor_col >= COLS) { cursor_col = 0; cursor_row++; }
            if (cursor_row < ROWS) {
                grid[cursor_row][cursor_col].ch = *s;
                grid[cursor_row][cursor_col].fg = cur_color;
                cursor_col++;
            }
        }
        s++;
    }
}

static void term_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void term_printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    term_print(buf);
}

/* Backend pointer: either the wl_shm window buffer or the selftest
 * anonymous buffer. render_terminal() is shared by both paths. */
static uint32_t *fb = NULL;

/* ------------------------------------------------------------------ */
/* Glyph renderer -- one pixel per font bit, replaces color blocks     */
/* ------------------------------------------------------------------ */
static void draw_char(int px, int py, char ch, uint32_t fg)
{
    if (ch < 0x20 || ch > 0x7E)
        ch = '?';
    const uint8_t *glyph = font5x7[ch - 0x20];
    for (int row = 0; row < GLYPH_H; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < GLYPH_W; col++) {
            if (bits & (0x10u >> col)) {
                int x = px + col, y = py + row;
                if (x >= 0 && x < WIN_W && y >= 0 && y < WIN_H)
                    fb[y * WIN_W + x] = fg;
            }
        }
    }
}

static void render_terminal(void)
{
    if (!fb)
        return;
    for (int i = 0; i < WIN_W * WIN_H; i++)
        fb[i] = COL_BG;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char ch = grid[r][c].ch;
            if (!ch)
                continue;
            draw_char(MARGIN_X + c * CELL_W, MARGIN_Y + r * CELL_H,
                      ch, grid[r][c].fg);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Wayland state                                                       */
/* ------------------------------------------------------------------ */
static struct wl_display *display = NULL;
static struct wl_compositor *compositor = NULL;
static struct wl_shm *shm = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct wl_surface *surface = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *toplevel = NULL;
static struct wl_buffer *buffer = NULL;
static int shm_fd = -1;
static uint32_t *wl_fb = NULL;
static int surface_configured = 0;

/* ---- wl_seat: was completely unbound before this fix ---- */
static struct wl_seat *seat = NULL;
static struct wl_pointer *pointer = NULL;
static struct wl_keyboard *keyboard = NULL;
static int has_pointer = 0, has_keyboard = 0;
static int ptr_x = -1, ptr_y = -1;
static int kb_last_key = 0;

static void pointer_enter(void *d, struct wl_pointer *p, uint32_t serial,
                          struct wl_surface *s, wl_fixed_t sx, wl_fixed_t sy)
{ (void)d;(void)p;(void)serial;(void)s; ptr_x = wl_fixed_to_int(sx); ptr_y = wl_fixed_to_int(sy); }
static void pointer_leave(void *d, struct wl_pointer *p, uint32_t serial,
                          struct wl_surface *s)
{ (void)d;(void)p;(void)serial;(void)s; ptr_x = ptr_y = -1; }
static void pointer_motion(void *d, struct wl_pointer *p, uint32_t t,
                           wl_fixed_t sx, wl_fixed_t sy)
{ (void)d;(void)p;(void)t; ptr_x = wl_fixed_to_int(sx); ptr_y = wl_fixed_to_int(sy); }
static void pointer_button(void *d, struct wl_pointer *p, uint32_t s,
                           uint32_t t, uint32_t b, uint32_t st)
{ (void)d;(void)p;(void)s;(void)t;(void)b;(void)st; }
static void pointer_axis(void *d, struct wl_pointer *p, uint32_t t,
                         uint32_t a, wl_fixed_t v)
{ (void)d;(void)p;(void)t;(void)a;(void)v; }
static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter, .leave = pointer_leave, .motion = pointer_motion,
    .button = pointer_button, .axis = pointer_axis,
};

static void keyboard_keymap(void *d, struct wl_keyboard *k, uint32_t f,
                            int32_t fd, uint32_t size)
{ (void)d;(void)k;(void)f; close(fd); (void)size; }
static void keyboard_enter(void *d, struct wl_keyboard *k, uint32_t s,
                           struct wl_surface *surf, struct wl_array *keys)
{ (void)d;(void)k;(void)s;(void)surf;(void)keys; }
static void keyboard_leave(void *d, struct wl_keyboard *k, uint32_t s,
                           struct wl_surface *surf)
{ (void)d;(void)k;(void)s;(void)surf; }
static void keyboard_key(void *d, struct wl_keyboard *k, uint32_t s, uint32_t t,
                         uint32_t key, uint32_t state)
{ (void)d;(void)k;(void)s;(void)t; if (state) kb_last_key = (int)key; }
static void keyboard_modifiers(void *d, struct wl_keyboard *k, uint32_t s,
                               uint32_t dg, uint32_t dg_l, uint32_t l, uint32_t lg)
{ (void)d;(void)k;(void)s;(void)dg;(void)dg_l;(void)l;(void)lg; }
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap, .enter = keyboard_enter, .leave = keyboard_leave,
    .key = keyboard_key, .modifiers = keyboard_modifiers,
};

static void seat_capabilities(void *data, struct wl_seat *s, uint32_t caps)
{
    (void)data;
    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        has_pointer = 1;
        pointer = wl_seat_get_pointer(s);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    }
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        has_keyboard = 1;
        keyboard = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    }
}
static void seat_name(void *data, struct wl_seat *s, const char *name)
{ (void)data;(void)s; (void)name; }
static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities, .name = seat_name,
};

/* ---- xdg-shell + registry ---- */
static void xdg_surface_configure(void *d, struct xdg_surface *xs, uint32_t serial)
{
    (void)d;
    xdg_surface_ack_configure(xs, serial);
    if (!surface_configured) { surface_configured = 1; render_terminal(); }
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure };

static void xdg_wm_base_ping(void *d, struct xdg_wm_base *b, uint32_t s)
{ (void)d; xdg_wm_base_pong(b, s); }
static const struct xdg_wm_base_listener xdg_wm_base_listener = { .ping = xdg_wm_base_ping };

static void registry_global(void *d, struct wl_registry *r, uint32_t name,
                            const char *iface, uint32_t ver)
{
    (void)d; (void)ver;
    if (strcmp(iface, "wl_compositor") == 0)
        compositor = wl_registry_bind(r, name, &wl_compositor_interface, 4);
    else if (strcmp(iface, "wl_shm") == 0)
        shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (strcmp(iface, "xdg_wm_base") == 0) {
        xdg_wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 3);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    } else if (strcmp(iface, "wl_seat") == 0) {
        /* PHASE DESKTOP-SHELL: bind the seat (v1) and track pointer + keyboard */
        seat = wl_registry_bind(r, name, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }
}
static void registry_global_remove(void *d, struct wl_registry *r, uint32_t n)
{ (void)d;(void)r;(void)n; }
static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove,
};

static struct wl_buffer *create_buffer(void)
{
    char template[] = "/tmp/wlt-set-shm-XXXXXX";
    shm_fd = mkostemp(template, O_CLOEXEC);
    if (shm_fd < 0) return NULL;
    unlink(template);
    if (ftruncate(shm_fd, WIN_W * WIN_H * 4) < 0) { close(shm_fd); return NULL; }
    wl_fb = mmap(NULL, WIN_W * WIN_H * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (wl_fb == MAP_FAILED) { close(shm_fd); return NULL; }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, shm_fd, WIN_W * WIN_H * 4);
    struct wl_buffer *buf =
        wl_shm_pool_create_buffer(pool, 0, WIN_W, WIN_H, WIN_W * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Settings engine + live system data                                  */
/* ------------------------------------------------------------------ */
static wlt_settings_t *g_settings = NULL;

extern const wlt_panel_t wlt_display_panel;
extern const wlt_panel_t wlt_network_panel;
extern const wlt_panel_t wlt_privacy_panel;
extern const wlt_panel_t wlt_security_panel;

static void put_kv_bool(const char *key)
{
    bool v = false;
    if (wlt_settings_get_bool(g_settings, key, &v) == WLT_SETTINGS_OK)
        term_printf("  %s = %s\n", key, v ? "true" : "false");
}

static void put_kv_int(const char *key)
{
    int64_t v = 0;
    if (wlt_settings_get_int(g_settings, key, &v) == WLT_SETTINGS_OK)
        term_printf("  %s = %lld\n", key, (long long)v);
}

static void put_kv_str(const char *key)
{
    char v[128];
    if (wlt_settings_get_string(g_settings, key, v, sizeof(v)) == WLT_SETTINGS_OK)
        term_printf("  %s = %s\n", key, v);
}

static void read_system_data(void)
{
    char buf[256];
    memset(grid, 0, sizeof(grid));
    cursor_row = 0; cursor_col = 0;

    /* Header */
    term_color(COL_HDR);
    term_print("WLTIOS Settings\n");
    term_print("===============\n\n");

    /* Input status (from the newly-bound wl_seat) */
    term_color(COL_INPUT);
    if (seat) {
        term_printf("input: seat bound  pointer=%s", has_pointer ? "yes" : "no");
        if (has_pointer && ptr_x >= 0)
            term_printf(" @(%d,%d)", ptr_x, ptr_y);
        term_printf("  keyboard=%s\n\n", has_keyboard ? "yes" : "no");
    } else {
        term_print("input: no seat (headless / selftest)\n\n");
    }

    if (g_settings) {
        /* ---- Persisted settings, per panel ---- */
        term_color(COL_SECTION);
        term_print("[Display panel]\n");
        term_color(COL_VALUE);
        put_kv_str("display.resolution");
        put_kv_str("display.orientation");
        put_kv_int("display.brightness");
        put_kv_int("display.refresh_rate");
        put_kv_str("display.multi_monitor.mode");
        put_kv_str("display.scaling");
        put_kv_bool("display.night_light.enabled");
        term_print("\n");

        term_color(COL_SECTION);
        term_print("[Network panel]\n");
        term_color(COL_VALUE);
        put_kv_bool("network.tor.enabled");
        put_kv_int("network.tor.socks_port");
        put_kv_int("network.tor.control_port");
        put_kv_bool("network.proxy.enabled");
        put_kv_str("network.proxy.url");
        put_kv_bool("network.mac.randomization");
        term_print("\n");

        term_color(COL_SECTION);
        term_print("[Privacy panel]\n");
        term_color(COL_VALUE);
        put_kv_bool("privacy.location.enabled");
        put_kv_bool("privacy.camera.disabled");
        put_kv_bool("privacy.microphone.muted");
        put_kv_bool("privacy.clipboard.isolation");
        put_kv_bool("privacy.history.clear_on_exit");
        put_kv_bool("privacy.telemetry.enabled");
        put_kv_str("privacy.tor.circuit_mode");
        term_print("\n");

        term_color(COL_SECTION);
        term_print("[Security panel]\n");
        term_color(COL_VALUE);
        put_kv_bool("security.firewall.enabled");
        put_kv_str("security.firewall.policy");
        put_kv_bool("security.sandbox.enabled");
        put_kv_str("security.sandbox.level");
        put_kv_bool("security.disk_encryption.enabled");
        put_kv_bool("security.usb.guard");
        put_kv_bool("security.verified_boot.enabled");
        term_print("\n");

        term_color(COL_DIM);
        term_printf("engine: %zu settings registered, persisted via /etc/wlt/*.conf\n",
                    wlt_settings_count(g_settings));
        term_color(COL_WARN);
        term_print("live enactment: pending compositor IPC (apply = UNREACHABLE)\n\n");
    }

    /* ---- Live system status (as before) ---- */
    term_color(COL_SECTION);
    term_print("[Live network]\n");
    term_color(COL_TEXT);
    FILE *f = fopen("/proc/net/fib_trie", "r");
    if (f) {
        while (fgets(buf, sizeof(buf), f))
            if (strstr(buf, "10.0.2.")) { term_print("  IP: "); term_print(buf); break; }
        fclose(f);
    } else term_print("  IP: unavailable\n");
    f = fopen("/sys/class/net/eth0/carrier", "r");
    if (f) { char c[4]; if (fgets(c, sizeof(c), f)) { term_print("  eth0 carrier: "); term_print(c); } fclose(f); }
    else term_print("  eth0 carrier: unknown\n");
    if (access("/run/tor.pid", F_OK) == 0 || access("/var/lib/tor", F_OK) == 0)
        term_print("  Tor: ACTIVE\n");
    else term_print("  Tor: INACTIVE\n");
    if (access("/run/wlt-killswitch", F_OK) == 0)
        term_print("  Kill switch: ACTIVE\n");
    else term_print("  Kill switch: OFF\n");
    term_print("\n");

    term_color(COL_SECTION);
    term_print("[Live security]\n");
    term_color(COL_TEXT);
    int gs = socket(AF_UNIX, SOCK_STREAM, 0);
    if (gs >= 0) {
        struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/guardian.sock", sizeof(addr.sun_path) - 1);
        if (connect(gs, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            char resp[512] = {0};
            int n = (int)read(gs, resp, sizeof(resp) - 1);
            if (n > 0) { term_print("  Guardian: "); term_print(resp); }
            else term_print("  Guardian: online (no response)\n");
        } else term_print("  Guardian: socket unreachable\n");
        close(gs);
    } else term_print("  Guardian: cannot create socket\n");
    f = fopen("/etc/wltios-release", "r");
    if (f) { if (fgets(buf, sizeof(buf), f)) { term_print("  "); term_print(buf); } fclose(f); }
    term_print("\n");

    term_color(COL_SECTION);
    term_print("[Live system]\n");
    term_color(COL_TEXT);
    f = fopen("/proc/version", "r");
    if (f) { if (fgets(buf, sizeof(buf), f)) { term_print("  "); term_print(buf); } fclose(f); }
    f = fopen("/proc/uptime", "r");
    if (f) {
        double up;
        if (fscanf(f, "%lf", &up) == 1) {
            snprintf(buf, sizeof(buf), "  Uptime: %.0f seconds\n", up);
            term_print(buf);
        }
        fclose(f);
    }
    f = fopen("/proc/meminfo", "r");
    if (f) { if (fgets(buf, sizeof(buf), f)) { term_print("  "); term_print(buf); } fclose(f); }
}

/* Build the settings engine: create, register all four panels, load. */
static int init_settings_engine(void)
{
    if (wlt_settings_create(&g_settings) != WLT_SETTINGS_OK) {
        fprintf(stderr, "[wlt-settings] engine create failed\n");
        return -1;
    }
    int rc = 0;
    if (wlt_settings_register_panel(g_settings, &wlt_display_panel)  != WLT_SETTINGS_OK) rc = -1;
    if (wlt_settings_register_panel(g_settings, &wlt_network_panel)  != WLT_SETTINGS_OK) rc = -1;
    if (wlt_settings_register_panel(g_settings, &wlt_privacy_panel)  != WLT_SETTINGS_OK) rc = -1;
    if (wlt_settings_register_panel(g_settings, &wlt_security_panel) != WLT_SETTINGS_OK) rc = -1;
    if (rc) {
        fprintf(stderr, "[wlt-settings] panel registration failed\n");
        return -1;
    }
    wlt_settings_err_t lr = wlt_settings_load(g_settings);
    fprintf(stderr, "[wlt-settings] engine: %zu settings from 4 panels (load=%s)\n",
            wlt_settings_count(g_settings), wlt_settings_err_str(lr));
    return 0;
}

/* ------------------------------------------------------------------ */
/* Headless self-test: same grid, same renderer, no Wayland.           */
/* Verifies the font is real (glyph-like pixel variance on text rows)  */
/* and writes a PPM dump for human inspection.                         */
/* ------------------------------------------------------------------ */
static int selftest_main(void)
{
    fprintf(stderr, "[wlt-settings][selftest] headless render %dx%d\n", WIN_W, WIN_H);

    if (init_settings_engine() != 0)
        return 2;
    read_system_data();

    fb = mmap(NULL, (size_t)WIN_W * WIN_H * 4, PROT_READ | PROT_WRITE,
              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (fb == MAP_FAILED) { perror("mmap"); return 2; }

    render_terminal();

    /* PPM dump (ARGB8888 word: A<<24|R<<16|G<<8|B -> P6 wants R,G,B) */
    const char *ppm_path = "/tmp/wlt-settings-selftest.ppm";
    FILE *o = fopen(ppm_path, "wb");
    if (o) {
        fprintf(o, "P6\n%d %d\n255\n", WIN_W, WIN_H);
        for (int i = 0; i < WIN_W * WIN_H; i++) {
            uint32_t p = fb[i];
            fputc((int)((p >> 16) & 0xFF), o);
            fputc((int)((p >> 8)  & 0xFF), o);
            fputc((int)( p        & 0xFF), o);
        }
        fclose(o);
        fprintf(stderr, "[wlt-settings][selftest] PPM dump: %s\n", ppm_path);
    }

    /* Verification 1: background is present but not the only color */
    uint32_t colors[16] = {0};
    int distinct = 0;
    int fg_pixels = 0;
    for (int i = 0; i < WIN_W * WIN_H; i++) {
        uint32_t p = fb[i];
        if (p != COL_BG) fg_pixels++;
        int found = 0;
        for (int k = 0; k < distinct; k++) if (colors[k] == p) { found = 1; break; }
        if (!found && distinct < 16) colors[distinct++] = p;
    }
    /* Verification 2: INTERNAL HOLES. A glyph like 'O' has rows of the
     * form X...X -- background trapped BETWEEN two foreground runs. A
     * solid-block renderer can never produce that, and neither can an
     * all-zero font (no fg at all). Scan the title band "WLTIOS Settings". */
    int holes = 0;
    int title_fg = 0;
    for (int y = MARGIN_Y; y < MARGIN_Y + GLYPH_H; y++) {
        for (int x = MARGIN_X + 1; x < MARGIN_X + 16 * CELL_W - 1; x++) {
            uint32_t here = fb[y * WIN_W + x];
            uint32_t left = fb[y * WIN_W + x - 1];
            uint32_t right = fb[y * WIN_W + x + 1];
            if (here == COL_BG && left != COL_BG && right != COL_BG)
                holes++;
            if (here != COL_BG)
                title_fg++;
        }
    }

    fprintf(stderr, "[wlt-settings][selftest] stats: fg=%d distinct_colors=%d "
            "title_internal_holes=%d title_fg_px=%d\n",
            fg_pixels, distinct, holes, title_fg);

    int pass = 1;
    if (fg_pixels < 2000) {
        fprintf(stderr, "[wlt-settings][selftest] FAIL: too few fg pixels "
                "(font empty?)\n");
        pass = 0;
    }
    if (distinct < 3) {
        fprintf(stderr, "[wlt-settings][selftest] FAIL: <%d distinct colors "
                "(block renderer?)\n", distinct);
        pass = 0;
    }
    if (holes < 4) {
        fprintf(stderr, "[wlt-settings][selftest] FAIL: %d internal holes in "
                "title band -- solid blocks, not glyphs\n", holes);
        pass = 0;
    }
    fprintf(stderr, "[wlt-settings][selftest] %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
        return selftest_main();

    fprintf(stderr, "[wlt-settings] Starting\n");
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "[wlt-settings] Cannot connect to Wayland\n");
        return 1;
    }
    struct wl_registry *reg = wl_display_get_registry(display);
    wl_registry_add_listener(reg, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (!compositor || !shm || !xdg_wm_base) {
        fprintf(stderr, "[wlt-settings] Missing Wayland globals\n");
        return 1;
    }

    if (init_settings_engine() != 0)
        return 1;

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_set_title(toplevel, "WLTIOS Settings");
    xdg_toplevel_set_app_id(toplevel, "wlt-settings");

    buffer = create_buffer();
    if (!buffer) { fprintf(stderr, "[wlt-settings] Cannot create buffer\n"); return 1; }
    fb = wl_fb;

    read_system_data();
    render_terminal();

    fprintf(stderr, "[wlt-settings] Window created, entering event loop\n");
    while (wl_display_dispatch(display) != -1) {
        /* Re-render periodically so pointer coords / live status stay fresh */
        read_system_data();
        render_terminal();
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, WIN_W, WIN_H);
        wl_surface_commit(surface);
    }
    return 0;
}
