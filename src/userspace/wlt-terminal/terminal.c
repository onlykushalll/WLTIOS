/**
 * @file terminal.c
 * @brief Real Wayland terminal — xdg-shell window with keyboard input + command exec
 * @module wlt-terminal
 * @purpose A persistent terminal window where you can type commands and see output
 *
 * Build: gcc -O2 -Wall -DWLR_USE_UNSTABLE -D_GNU_SOURCE \
 *          $(pkg-config --cflags wayland-client xkbcommon) \
 *          terminal.c \
 *          $(pkg-config --libs wayland-client xkbcommon) \
 *          -o wlt-terminal
 * Run:   WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run wlt-terminal
 *
 * The terminal:
 *   - Creates an 800x600 xdg_toplevel window
 *   - Renders a character grid (100 cols x 37 rows) using bitmap font
 *   - Black bg, green text, white cursor
 *   - Keyboard input via wl_keyboard + xkbcommon
 *   - Enter executes the command line via fork+exec /bin/wlt-shell
 *   - Command output displayed in the terminal
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

/* XDG shell protocol — we need the unstable header */
#include "xdg-shell-client-protocol.h"

#define WIN_W 800
#define WIN_H 600
#define CHAR_W 8
#define CHAR_H 16
#define COLS (WIN_W / CHAR_W)
#define ROWS (WIN_H / CHAR_H)
#define MAX_INPUT 256

/* ─── Wayland globals ──────────────────────────────── */
struct wl_display *display = NULL;
struct wl_registry *registry = NULL;
struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct xdg_wm_base *xdg_wm_base = NULL;
struct wl_seat *seat = NULL;
struct wl_keyboard *keyboard = NULL;

/* Surfaces */
struct wl_surface *surface = NULL;
struct xdg_surface *xdg_surface = NULL;
struct xdg_toplevel *toplevel = NULL;
struct wl_buffer *buffer = NULL;

/* SHM buffer */
void *shm_data = NULL;
int shm_fd = -1;

/* Keyboard */
struct xkb_context *xkb_ctx = NULL;
struct xkb_keymap *xkb_keymap = NULL;
struct xkb_state *xkb_state = NULL;

/* Terminal state */
char grid[ROWS][COLS];
int cursor_col = 0;
int cursor_row = 0;
char input_buf[MAX_INPUT];
int surface_configured = 0;  /* set to 1 after first xdg_surface.configure */
int input_len = 0;
int running = 1;

/* ─── Bitmap font (5x7, scaled 2x to 10x14) ──────── */
/* Minimal font: each char is 5x7 bits, we'll use simple patterns */
static const uint8_t font[][7] = {
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!'] = {0x04,0x04,0x04,0x04,0x00,0x00,0x04},
    ['/'] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    ['0'] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    ['1'] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    ['2'] = {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    ['3'] = {0x1F,0x01,0x02,0x06,0x01,0x11,0x0E},
    ['4'] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    ['5'] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    ['6'] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    ['7'] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    ['8'] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    ['9'] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    [':'] = {0x00,0x04,0x00,0x00,0x00,0x04,0x00},
    ['a'] = {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
    ['b'] = {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
    ['c'] = {0x00,0x00,0x0E,0x10,0x10,0x10,0x0E},
    ['d'] = {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F},
    ['e'] = {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
    ['f'] = {0x03,0x04,0x1F,0x04,0x04,0x04,0x04},
    ['g'] = {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E},
    ['h'] = {0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
    ['i'] = {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
    ['l'] = {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
    ['n'] = {0x00,0x00,0x1E,0x11,0x11,0x11,0x11},
    ['o'] = {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
    ['r'] = {0x00,0x00,0x1E,0x11,0x10,0x10,0x10},
    ['s'] = {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},
    ['t'] = {0x08,0x08,0x1C,0x08,0x08,0x09,0x06},
    ['u'] = {0x00,0x00,0x11,0x11,0x11,0x11,0x0F},
    ['w'] = {0x00,0x00,0x11,0x11,0x11,0x1F,0x11},
    ['y'] = {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},
};

/* Draw a character at grid position (col, row) */
static void draw_char(int col, int row, char c, uint32_t *pixels)
{
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = font[(int)c];
    if (!glyph) glyph = font[(int)'?'];

    int px = col * CHAR_W;
    int py = row * CHAR_H;

    for (int gy = 0; gy < 7; gy++)
    {
        for (int gx = 0; gx < 5; gx++)
        {
            if (glyph[gy] & (0x10 >> gx))
            {
                /* Draw 2x2 block for each font pixel (scaled) */
                for (int dy = 0; dy < 2; dy++)
                {
                    for (int dx = 0; dx < 2; dx++)
                    {
                        int x = px + gx * 2 + dx;
                        int y = py + gy * 2 + dy;
                        if (x >= 0 && x < WIN_W && y >= 0 && y < WIN_H)
                            pixels[y * WIN_W + x] = 0xFF00FF88; /* green */
                    }
                }
            }
        }
    }
}

/* Render the entire terminal grid to the SHM buffer */
static void render_terminal(void)
{
    uint32_t *pixels = (uint32_t *)shm_data;

    /* Black background */
    memset(pixels, 0, WIN_W * WIN_H * 4);

    /* Draw all characters in the grid */
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (grid[r][c] != 0)
                draw_char(c, r, grid[r][c], pixels);
        }
    }

    /* Draw cursor (white block) */
    int cx = cursor_col * CHAR_W;
    int cy = cursor_row * CHAR_H;
    for (int y = 0; y < CHAR_H; y++)
    {
        for (int x = 0; x < CHAR_W; x++)
        {
            if (cx + x >= 0 && cx + x < WIN_W && cy + y >= 0 && cy + y < WIN_H)
                pixels[(cy + y) * WIN_W + (cx + x)] = 0xFFFFFFFF; /* white */
        }
    }

    /* Commit the surface — only after first configure event */
    if (!surface_configured) return;
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, WIN_W, WIN_H);
    wl_surface_commit(surface);
}

/* Put a character at cursor position and advance */
static void put_char(char c)
{
    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= ROWS)
        {
            /* Scroll up */
            memmove(grid, grid[1], (ROWS - 1) * COLS);
            memset(grid[ROWS - 1], 0, COLS);
            cursor_row = ROWS - 1;
        }
        return;
    }
    if (cursor_col >= COLS)
    {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= ROWS)
        {
            memmove(grid, grid[1], (ROWS - 1) * COLS);
            memset(grid[ROWS - 1], 0, COLS);
            cursor_row = ROWS - 1;
        }
    }
    grid[cursor_row][cursor_col] = c;
    cursor_col++;
}

/* Print a string to the terminal */
static void term_print(const char *str)
{
    while (*str)
        put_char(*str++);
}

/* Execute a command and display output */
static void execute_command(const char *cmd)
{
    term_print("\n");
    char shell_cmd[MAX_INPUT];
    snprintf(shell_cmd, sizeof(shell_cmd), "%s", cmd);

    /* Fork + exec /bin/wlt-shell with -c */
    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        term_print("error: pipe failed\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        /* Child: redirect stdout to pipe, exec command */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char *argv[] = {"/bin/wlt-shell", "-c", shell_cmd, NULL};
        char *envp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=linux", NULL};
        execve("/bin/wlt-shell", argv, envp);
        _exit(127);
    }

    close(pipefd[1]);

    /* Read output and display */
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        term_print(buf);
    }
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);
    term_print("\n");
}

/* ─── Wayland registry ─────────────────────────────── */
static void registry_global(void *data, struct wl_registry *reg,
                            uint32_t name, const char *interface, uint32_t version)
{
    (void)data; (void)version;
    if (strcmp(interface, "wl_compositor") == 0)
        compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    else if (strcmp(interface, "wl_shm") == 0)
        shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (strcmp(interface, "xdg_wm_base") == 0)
        xdg_wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 3);
    else if (strcmp(interface, "wl_seat") == 0)
    {
        seat = wl_registry_bind(reg, name, &wl_seat_interface, 4);
        keyboard = wl_seat_get_keyboard(seat);
    }
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
    (void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* ─── XDG surface ──────────────────────────────────── */
static void xdg_surface_configure(void *data, struct xdg_surface *xs, uint32_t serial)
{
    (void)data;
    xdg_surface_ack_configure(xs, serial);
    if (!surface_configured)
    {
        surface_configured = 1;
        term_print("WLTIOS Terminal v1.0\n");
        term_print("Type 'help' for commands.\n\n");
        term_print("/wlt# ");
        render_terminal();
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *t,
                                    int32_t width, int32_t height, struct wl_array *states)
{
    (void)data; (void)t; (void)width; (void)height; (void)states;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *t)
{
    (void)data; (void)t;
    running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

/* ─── Keyboard ─────────────────────────────────────── */
static void keyboard_keymap(void *data, struct wl_keyboard *kb,
                             uint32_t format, int32_t fd, uint32_t size)
{
    (void)data; (void)kb;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }

    char *map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) { close(fd); return; }

    xkb_keymap = xkb_keymap_new_from_string(xkb_ctx, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    xkb_state = xkb_state_new(xkb_keymap);
    munmap(map_str, size);
    close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *kb,
                            uint32_t serial, struct wl_surface *surf, struct wl_array *keys)
{
    (void)data; (void)kb; (void)serial; (void)surf; (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surf)
{
    (void)data; (void)kb; (void)serial; (void)surf;
}

static void keyboard_key(void *data, struct wl_keyboard *kb,
                          uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    (void)data; (void)kb; (void)serial; (void)time;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    if (!xkb_state) return;

    xkb_keysym_t sym = xkb_state_key_get_one_sym(xkb_state, key + 8);

    if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter)
    {
        input_buf[input_len] = '\0';
        term_print(input_buf);
        execute_command(input_buf);
        input_len = 0;
        input_buf[0] = '\0';
        term_print("/wlt# ");
        render_terminal();
        return;
    }

    if (sym == XKB_KEY_BackSpace)
    {
        if (input_len > 0)
        {
            input_len--;
            input_buf[input_len] = '\0';
            if (cursor_col > 0) cursor_col--;
            grid[cursor_row][cursor_col] = 0;
        }
        render_terminal();
        return;
    }

    char buf[8];
    int len = xkb_keysym_to_utf8(sym, buf, sizeof(buf));
    if (len > 0 && input_len < MAX_INPUT - 1)
    {
        input_buf[input_len++] = buf[0];
        input_buf[input_len] = '\0';
        put_char(buf[0]);
        render_terminal();
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *kb,
                                uint32_t serial, uint32_t mods_depressed,
                                uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    (void)data; (void)kb; (void)serial;
    if (xkb_state)
        xkb_state_update_mask(xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
};

/* ─── SHM buffer creation ──────────────────────────── */
static struct wl_buffer *create_buffer(void)
{
    char template[] = "/tmp/wlt-term-shm-XXXXXX";
    shm_fd = mkostemp(template, O_CLOEXEC);
    if (shm_fd < 0) return NULL;
    unlink(template);

    if (ftruncate(shm_fd, WIN_W * WIN_H * 4) < 0) { close(shm_fd); return NULL; }

    shm_data = mmap(NULL, WIN_W * WIN_H * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) { close(shm_fd); return NULL; }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, shm_fd, WIN_W * WIN_H * 4);
    struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, WIN_W, WIN_H, WIN_W * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    return buf;
}

/* ─── Main ─────────────────────────────────────────── */
int main(void)
{
    fprintf(stderr, "[wlt-terminal] Starting\n");

    display = wl_display_connect(NULL);
    if (!display)
    {
        fprintf(stderr, "[wlt-terminal] Cannot connect to Wayland display\n");
        return 1;
    }
    fprintf(stderr, "[wlt-terminal] Connected to Wayland\n");

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !xdg_wm_base)
    {
        fprintf(stderr, "[wlt-terminal] Missing Wayland globals\n");
        return 1;
    }

    xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);

    /* Create xkb context */
    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    /* Create surface + xdg toplevel */
    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(toplevel, "WLTIOS Terminal");
    xdg_toplevel_set_app_id(toplevel, "wlt-terminal");

    /* Create SHM buffer */
    buffer = create_buffer();
    if (!buffer)
    {
        fprintf(stderr, "[wlt-terminal] Cannot create buffer\n");
        return 1;
    }

    /* Initialize terminal display — actual render deferred to xdg_surface.configure */
    memset(grid, 0, sizeof(grid));

    /* Setup keyboard */
    if (keyboard)
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);

    fprintf(stderr, "[wlt-terminal] Window created, entering event loop\n");

    /* Main loop */
    while (running)
    {
        wl_display_dispatch(display);
    }

    fprintf(stderr, "[wlt-terminal] Exiting\n");
    wl_buffer_destroy(buffer);
    if (toplevel) xdg_toplevel_destroy(toplevel);
    if (xdg_surface) xdg_surface_destroy(xdg_surface);
    if (surface) wl_surface_destroy(surface);
    wl_display_disconnect(display);

    return 0;
}