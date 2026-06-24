/**
 * @file comp_main.c
 * @brief Production compositor main — runs forever (no 3s timeout)
 * @module wlt-compositor
 * @purpose Entry point for the WLTIOS Wayland compositor
 *
 * Unlike comp_test_main.c (which exits after 3s), this main blocks
 * forever in wlt_comp_run() until wl_display_terminate() is called.
 *
 * Build: gcc -DWLR_USE_UNSTABLE -Iinclude compositor.c comp_main.c \
 *          $(pkg-config --cflags --libs wlroots-0.18 wayland-server) -o wlt-compositor
 */

#include "compositor.h"
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t g_running = 1;

static void on_sigterm(int sig) {
    (void)sig;
    g_running = 0;
}

int main(void) {
    fprintf(stderr, "[compositor] WLTIOS compositor starting\n");

    struct sigaction sa = {0};
    sa.sa_handler = on_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

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