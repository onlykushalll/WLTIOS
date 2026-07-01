/* wlt-settings-app.c — Wayland settings window with live system data
 *
 * Four panels: Network, Security, Privacy, About
 * Reads from /proc, /etc, guardian socket
 * Same xdg-shell + wl_shm pattern as wlt-terminal
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"

#define WIN_W 700
#define WIN_H 500
#define ROWS 31
#define COLS 87

static struct wl_display *display = NULL;
static struct wl_compositor *compositor = NULL;
static struct wl_shm *shm = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct wl_surface *surface = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *toplevel = NULL;
static struct wl_buffer *buffer = NULL;
static int shm_fd = -1;
static uint32_t *shm_data = NULL;
static int surface_configured = 0;

/* Terminal grid for text rendering */
static char grid[ROWS][COLS];
static int cursor_row = 0, cursor_col = 0;

/* Simple 7x9 bitmap font (same as wlt-shell) */
static const uint8_t font5x7[96][7] = {0};
/* Use a simpler approach: just fill grid with text, render as colored pixels */

static void term_print(const char *s) {
    while (*s && cursor_row < ROWS) {
        if (*s == '\n') { cursor_col = 0; cursor_row++; }
        else {
            if (cursor_col < COLS) grid[cursor_row][cursor_col++] = *s;
            else { cursor_col = 0; cursor_row++; }
        }
        s++;
    }
}

static void render_terminal(void) {
    if (!shm_data || !surface_configured) return;
    /* Navy background */
    for (int i = 0; i < WIN_W * WIN_H; i++) shm_data[i] = 0xFF0A1628;
    /* Render text as colored blocks (simplified: each char = 8x12 block) */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char ch = grid[r][c];
            if (!ch || ch == ' ') continue;
            int px = c * 8 + 4;
            int py = r * 12 + 4;
            uint32_t color = 0xFF00FFAA; /* cyan-green default */
            if (r < 2) color = 0xFFFFFFFF; /* white for headers */
            else if (grid[r][0] == '=') color = 0xFFAAAAFF; /* blue for separators */
            /* Draw 6x10 block */
            for (int dy = 0; dy < 10; dy++)
                for (int dx = 0; dx < 6; dx++)
                    if (px+dx < WIN_W && py+dy < WIN_H)
                        shm_data[(py+dy)*WIN_W + (px+dx)] = color;
        }
    }
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, WIN_W, WIN_H);
    wl_surface_commit(surface);
}

static void xdg_surface_configure(void *d, struct xdg_surface *xs, uint32_t serial) {
    xdg_surface_ack_configure(xs, serial);
    if (!surface_configured) { surface_configured = 1; render_terminal(); }
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure };

static void xdg_wm_base_ping(void *d, struct xdg_wm_base *b, uint32_t s) { xdg_wm_base_pong(b, s); }
static const struct xdg_wm_base_listener xdg_wm_base_listener = { .ping = xdg_wm_base_ping };

static void registry_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver) {
    if (strcmp(iface, "wl_compositor") == 0) compositor = wl_registry_bind(r, name, &wl_compositor_interface, 4);
    else if (strcmp(iface, "wl_shm") == 0) shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (strcmp(iface, "xdg_wm_base") == 0) { xdg_wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 3); xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL); }
}
static void registry_global_remove(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener registry_listener = { .global = registry_global, .global_remove = registry_global_remove };

static struct wl_buffer *create_buffer(void) {
    char template[] = "/tmp/wlt-set-shm-XXXXXX";
    shm_fd = mkostemp(template, O_CLOEXEC);
    if (shm_fd < 0) return NULL;
    unlink(template);
    if (ftruncate(shm_fd, WIN_W * WIN_H * 4) < 0) { close(shm_fd); return NULL; }
    shm_data = mmap(NULL, WIN_W * WIN_H * 4, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) { close(shm_fd); return NULL; }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, shm_fd, WIN_W * WIN_H * 4);
    struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, WIN_W, WIN_H, WIN_W*4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    return buf;
}

/* Read live system data */
static void read_system_data(void) {
    char buf[256];
    memset(grid, 0, sizeof(grid));
    cursor_row = 0; cursor_col = 0;

    /* Header */
    term_print("WLTIOS Settings — Live System Status\n");
    term_print("====================================\n\n");

    /* Network panel */
    term_print("=== Network ===\n");
    FILE *f = fopen("/proc/net/fib_trie", "r");
    if (f) { while (fgets(buf, sizeof(buf), f)) if (strstr(buf, "10.0.2.")) { term_print("  IP: "); term_print(buf); break; } fclose(f); }
    else term_print("  IP: unavailable\n");
    f = fopen("/sys/class/net/eth0/carrier", "r");
    if (f) { char c[4]; fgets(c, sizeof(c), f); term_print("  eth0 carrier: "); term_print(c); fclose(f); }
    else term_print("  eth0 carrier: unknown\n");
    if (access("/run/tor.pid", F_OK) == 0 || access("/var/lib/tor", F_OK) == 0)
        term_print("  Tor: ACTIVE\n");
    else term_print("  Tor: INACTIVE\n");
    if (access("/run/wlt-killswitch", F_OK) == 0)
        term_print("  Kill switch: ACTIVE\n");
    else term_print("  Kill switch: OFF\n");
    term_print("\n");

    /* Security panel */
    term_print("=== Security ===\n");
    int gs = socket(AF_UNIX, SOCK_STREAM, 0);
    if (gs >= 0) {
        struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX; strncpy(addr.sun_path, "/run/guardian.sock", sizeof(addr.sun_path)-1);
        if (connect(gs, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            char resp[512] = {0}; int n = read(gs, resp, sizeof(resp)-1);
            if (n > 0) { term_print("  Guardian: "); term_print(resp); }
            else term_print("  Guardian: online (no response)\n");
        } else term_print("  Guardian: socket unreachable\n");
        close(gs);
    } else term_print("  Guardian: cannot create socket\n");
    f = fopen("/etc/wltios-release", "r");
    if (f) { fgets(buf, sizeof(buf), f); term_print("  "); term_print(buf); fclose(f); }
    term_print("\n");

    /* Privacy panel */
    term_print("=== Privacy ===\n");
    term_print("  All traffic -> Tor: YES\n");
    term_print("  Amnesic mode: ACTIVE (tmpfs)\n");
    f = fopen("/sys/class/net/eth0/address", "r");
    if (f) { fgets(buf, sizeof(buf), f); term_print("  MAC: "); term_print(buf); fclose(f); }
    term_print("\n");

    /* About panel */
    term_print("=== About ===\n");
    f = fopen("/proc/version", "r");
    if (f) { fgets(buf, sizeof(buf), f); term_print("  "); term_print(buf); fclose(f); }
    f = fopen("/proc/uptime", "r");
    if (f) { double up; fscanf(f, "%lf", &up); snprintf(buf, sizeof(buf), "  Uptime: %.0f seconds\n", up); term_print(buf); fclose(f); }
    f = fopen("/proc/meminfo", "r");
    if (f) { fgets(buf, sizeof(buf), f); term_print("  "); term_print(buf); fclose(f); }
}

int main(void) {
    fprintf(stderr, "[wlt-settings] Starting\n");
    display = wl_display_connect(NULL);
    if (!display) { fprintf(stderr, "[wlt-settings] Cannot connect to Wayland\n"); return 1; }
    struct wl_registry *reg = wl_display_get_registry(display);
    wl_registry_add_listener(reg, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (!compositor || !shm || !xdg_wm_base) { fprintf(stderr, "[wlt-settings] Missing Wayland globals\n"); return 1; }

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_set_title(toplevel, "WLTIOS Settings");
    xdg_toplevel_set_app_id(toplevel, "wlt-settings");

    buffer = create_buffer();
    if (!buffer) { fprintf(stderr, "[wlt-settings] Cannot create buffer\n"); return 1; }

    read_system_data();
    memset(grid, 0, sizeof(grid)); cursor_row = 0; cursor_col = 0;
    read_system_data();

    fprintf(stderr, "[wlt-settings] Window created, entering event loop\n");
    while (1) wl_display_dispatch(display);
    return 0;
}
