/**
 * @file comp_main.c
 * @brief Production compositor main — renders desktop + status bar, runs forever
 * @module wlt-compositor
 *
 * v2: Added framebuffer desktop rendering (navy bg + status bar with clock + Tor status)
 * before entering the wlroots event loop.
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

    /* Draw "WLTIOS 0.1.0" on the left of status bar (white pixels) */
    /* Simple 5x7 font for each character */
    const char *title = "WLTIOS 0.1.0";
    int tx = 8, ty = bar_y + 12;
    for (const char *c = title; *c; c++)
    {
        /* Draw each character as a simple block pattern */
        if (*c == ' ')
        {
            tx += 8;
            continue;
        }
        /* Draw a simple 5x7 dot pattern for each char */
        for (int dy = 0; dy < 7; dy++)
        {
            for (int dx = 0; dx < 5; dx++)
            {
                /* Simple pattern: all chars get a basic block outline */
                int px = tx + dx, py = ty + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                {
                    uint8_t *p = fb_mem + (py * line_len) + (px * bytes_pp);
                    /* Only draw if the char is not a space/dot */
                    if (*c != '.' && *c != ' ')
                    {
                        p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF; /* white */
                        if (bytes_pp == 4) p[3] = 0xFF;
                    }
                }
            }
        }
        tx += 8;
    }

    /* Draw "TOR: ON" on the right side (green if tor.pid exists) */
    const char *tor_text = "TOR: ON";
    int tor_x = fb_w - 80;
    int tor_y = bar_y + 12;

    /* Check if Tor is running */
    int tor_running = (access("/run/tor.pid", F_OK) == 0) ||
                      (access("/var/lib/tor", F_OK) == 0);

    uint8_t tr, tg, tb;
    if (tor_running) { tr = 0x00; tg = 0xFF; tb = 0x88; } /* green */
    else { tr = 0xFF; tg = 0x44; tb = 0x44; } /* red */

    for (const char *c = tor_text; *c; c++)
    {
        if (*c == ' ' || *c == ':')
        {
            tor_x += 8;
            continue;
        }
        for (int dy = 0; dy < 7; dy++)
        {
            for (int dx = 0; dx < 5; dx++)
            {
                int px = tor_x + dx, py = tor_y + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                {
                    uint8_t *p = fb_mem + (py * line_len) + (px * bytes_pp);
                    p[0] = tb; p[1] = tg; p[2] = tr;
                    if (bytes_pp == 4) p[3] = 0xFF;
                }
            }
        }
        tor_x += 8;
    }

    /* Draw a clock in center (HH:MM:SS from /proc/uptime) */
    FILE *up = fopen("/proc/uptime", "r");
    if (up)
    {
        double uptime;
        fscanf(up, "%lf", &uptime);
        fclose(up);
        unsigned int sec = (unsigned int)uptime;
        unsigned int h = sec / 3600;
        unsigned int m = (sec % 3600) / 60;
        unsigned int s = sec % 60;
        char clock_str[16];
        snprintf(clock_str, sizeof(clock_str), "%02u:%02u:%02u", h, m, s);

        int clk_x = (fb_w - 48) / 2;
        int clk_y = bar_y + 12;
        for (const char *c = clock_str; *c; c++)
        {
            if (*c == ':')
            {
                clk_x += 8;
                continue;
            }
            for (int dy = 0; dy < 7; dy++)
            {
                for (int dx = 0; dx < 5; dx++)
                {
                    int px = clk_x + dx, py = clk_y + dy;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    {
                        uint8_t *p = fb_mem + (py * line_len) + (px * bytes_pp);
                        p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF; /* white */
                        if (bytes_pp == 4) p[3] = 0xFF;
                    }
                }
            }
            clk_x += 8;
        }
    }

    munmap(fb_mem, fb_size);
    close(fb);
    fprintf(stderr, "[compositor] Desktop rendered (navy bg + status bar + clock + Tor status)\n");
}

int main(void) {
    fprintf(stderr, "[compositor] WLTIOS compositor starting\n");

    struct sigaction sa = {0};
    sa.sa_handler = on_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Render desktop background + status bar to /dev/fb0 before starting compositor */
    render_desktop();

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