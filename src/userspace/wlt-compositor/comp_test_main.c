/**
 * @file comp_test_main.c
 * @brief Standalone test main for the compositor — runs headless for 3s
 * @module wlt-compositor
 * @purpose Verify the compositor initializes and runs without crashing
 *
 * Build: gcc -DWLR_USE_UNSTABLE compositor.c comp_test_main.c \
 *          $(pkg-config --cflags --libs wlroots-0.18 wayland-server) -o comp-test
 * Run:   ./comp-test
 *
 * Expected: prints init messages, runs for 3 seconds, exits cleanly.
 */

#include "compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static volatile sig_atomic_t g_running = 1;

static void on_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

int main(void) {
    fprintf(stderr, "[comp-test] Starting WLTIOS compositor headless test\n");

    /* Set up signal handler for clean exit */
    struct sigaction sa = {0};
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    wlt_comp_state_t state;
    wlt_comp_error_t err = wlt_comp_init(&state);
    if (err != WLT_COMP_OK) {
        fprintf(stderr, "[comp-test] wlt_comp_init failed: %s\n",
                wlt_comp_error_str(err));
        return 1;
    }

    fprintf(stderr, "[comp-test] Init OK, running for 3 seconds...\n");

    /* Run the compositor in a child process so we can time-limit it */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: run the compositor (blocks) */
        wlt_comp_run(&state);
        _exit(0);
    } else if (pid > 0) {
        /* Parent: wait 3s then kill child */
        sleep(3);
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, 0);
        fprintf(stderr, "[comp-test] Child exited (status=%d)\n", status);
    } else {
        fprintf(stderr, "[comp-test] fork() failed\n");
        wlt_comp_shutdown(&state);
        return 1;
    }

    wlt_comp_shutdown(&state);
    fprintf(stderr, "[comp-test] SUCCESS — compositor ran for 3s without crashing\n");
    return 0;
}