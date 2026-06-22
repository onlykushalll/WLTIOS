/**
 * @file shell.c
 * @brief Main shell logic — Wayland layer-shell, event loop, IPC dispatch
 * @module wlt-shell
 * @purpose Desktop shell entry point, coordinates taskbar/launcher/notifications/tray
 * @dependencies: shell.h
 * @tested_by: tests/userspace/shell_test.c
 * @spec: docs/specs/wlt-shell.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

wlt_shell_error_t
wlt_shell_init(wlt_shell_state_t *state)
{
    if (state == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));

    /* Initialize tray statuses */
    strncpy(state->tray_status[WLT_SHELL_TRAY_TOR], "inactive",
            sizeof(state->tray_status[0]) - 1);
    strncpy(state->tray_status[WLT_SHELL_TRAY_NETWORK], "disconnected",
            sizeof(state->tray_status[0]) - 1);
    strncpy(state->tray_status[WLT_SHELL_TRAY_BATTERY], "unknown",
            sizeof(state->tray_status[0]) - 1);
    strncpy(state->tray_status[WLT_SHELL_TRAY_CLOCK], "00:00",
            sizeof(state->tray_status[0]) - 1);

    state->launcher_visible = false;
    state->launcher_query[0] = '\0';
    state->show_hidden = false;

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_run(wlt_shell_state_t *state)
{
    if (state == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    /* TODO(wlt-shell): Connect to Wayland compositor via layer-shell */
    /* TODO(wlt-shell): Set up IPC socket to WLT-Init */
    /* TODO(wlt-shell): Enter main event loop (poll Wayland fd + IPC fd) */

    /* For now, this is a stub — the real implementation needs wlroots */
    /* which must be built inside WSL2 Linux */

    while (1)
    {
        /* Update clock */
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        snprintf(state->tray_status[WLT_SHELL_TRAY_CLOCK],
                 sizeof(state->tray_status[0]),
                 "%02d:%02d", tm->tm_hour, tm->tm_min);

        /* Poll for events (TODO: Wayland + IPC) */
        usleep(100000);  /* 100ms */
    }

    return WLT_SHELL_OK;
}

void
wlt_shell_shutdown(wlt_shell_state_t *state)
{
    if (state == NULL)
        return;

    /* Clean up resources */
    memset(state, 0, sizeof(*state));
}

const char *
wlt_shell_error_str(wlt_shell_error_t err)
{
    switch (err)
    {
        case WLT_SHELL_OK:              return "OK";
        case WLT_SHELL_ERR_INVALID_ARG: return "Invalid argument";
        case WLT_SHELL_ERR_IO:          return "I/O error";
        case WLT_SHELL_ERR_NOMEM:       return "Out of memory";
        case WLT_SHELL_ERR_NOT_FOUND:   return "Not found";
        case WLT_SHELL_ERR_PERMISSION:  return "Permission denied";
        case WLT_SHELL_ERR_COMPOSITOR:  return "Compositor error";
        default:                        return "Unknown error";
    }
}
