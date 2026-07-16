/**
 * @file comp_main.c
 * @brief Production compositor main — renders desktop + status bar, runs forever
 * @module wlt-compositor
 *
 * v3: Removed fb0 early-exit — let the wlroots compositor run with DRM backend.
 *     The desktop background is still rendered to fb0 first (for kernel fbcon
 *     text visibility during boot), but the compositor then takes over via DRM.
 */

#include "compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <time.h>

static volatile sig_atomic_t g_running = 1;

static void on_sigterm(int sig) {
    (void)sig;
    g_running = 0;
}

/* Render a simple desktop background + status bar to /dev/fb0 */
static void render_desktop(void)
{
    int fb = open("/dev/fb0", O_RDWR);
    if (fb < 0)
    {
        fprintf(stderr, "[compositor] No /dev/fb0 — headless only\n");
        return;
    }

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    if (ioctl(fb, FBIOGET_VSCREENINFO, &var) < 0 ||
        ioctl(fb, FBIOGET_FSCREENINFO, &fix) < 0)
    {
        fprintf(stderr, "[compositor] Cannot get fb info\n");
        close(fb);
        return;
    }

    int fb_w = var.xres;
    int fb_h = var.yres;
    int bpp = var.bits_per_pixel;
    int bytes_pp = bpp / 8;
    int line_len = fix.line_length;
    size_t fb_size = fix.smem_len;

    fprintf(stderr, "[compositor] Rendering desktop: %dx%d %dbpp\n", fb_w, fb_h, bpp);

    uint8_t *fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (fb_mem == MAP_FAILED)
    {
        fprintf(stderr, "[compositor] mmap failed\n");
        close(fb);
        return;
    }

    /* Fill background with navy (0x1a1a2e in RGB) */
    uint8_t bg_r = 0x1a, bg_g = 0x1a, bg_b = 0x2e;
    for (int y = 0; y < fb_h; y++)
    {
        for (int x = 0; x < fb_w; x++)
        {
            uint8_t *p = fb_mem + (y * line_len) + (x * bytes_pp);
            p[0] = bg_b; p[1] = bg_g; p[2] = bg_r;
            if (bytes_pp == 4) p[3] = 0xFF;
        }
    }

    /* Draw status bar at bottom (32px high, dark gray 0x2a2a3e) */
    int bar_h = 32;
    int bar_y = fb_h - bar_h;
    uint8_t bar_r = 0x2a, bar_g = 0x2a, bar_b = 0x3e;
    for (int y = bar_y; y < fb_h; y++)
    {
        for (int x = 0; x < fb_w; x++)
        {
            uint8_t *p = fb_mem + (y * line_len) + (x * bytes_pp);
            p[0] = bar_b; p[1] = bar_g; p[2] = bar_r;
            if (bytes_pp == 4) p[3] = 0xFF;
        }
    }

    munmap(fb_mem, fb_size);
    close(fb);
    fprintf(stderr, "[compositor] Desktop rendered (navy bg + status bar)\n");
}

int main(void) {
    setenv("LD_LIBRARY_PATH", "/usr/lib/x86_64-linux-gnu:/lib:/usr/lib", 1);
    setenv("LIBGL_DRIVERS_PATH", "/usr/lib/x86_64-linux-gnu/dri", 1);
    setenv("WLR_RENDERER", "pixman", 1);
    fprintf(stderr, "[compositor] env: LD_LIBRARY_PATH=%s LIBGL_DRIVERS_PATH=%s\n",
            getenv("LD_LIBRARY_PATH"), getenv("LIBGL_DRIVERS_PATH"));

    fprintf(stderr, "[compositor] WLTIOS compositor starting (v3 — DRM output test)\n");

    struct sigaction sa = {0};
    sa.sa_handler = on_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Render desktop background to fb0 first (for boot visibility) */
    render_desktop();

    /* v3: NO fb0 early-exit — fall through to wlroots compositor.
     * The compositor will create a DRM backend (if /dev/dri/card0 exists)
     * and drive the display via DRM scanout. The fb0 desktop render above
     * is just a fallback that shows during boot before the compositor
     * takes over. */

    wlt_comp_state_t state;
    wlt_comp_error_t err = wlt_comp_init(&state);
    if (err != WLT_COMP_OK) {
        fprintf(stderr, "[compositor] init failed: %s\n", wlt_comp_error_str(err));
        return 1;
    }

    fprintf(stderr, "[compositor] Init OK, entering event loop\n");

    err = wlt_comp_run(&state);
    if (err != WLT_COMP_OK) {
        fprintf(stderr, "[compositor] run failed: %s\n", wlt_comp_error_str(err));
    }

    wlt_comp_shutdown(&state);
    fprintf(stderr, "[compositor] Shutdown complete\n");
    return 0;
}
