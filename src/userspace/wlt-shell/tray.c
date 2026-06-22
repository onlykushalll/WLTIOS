/**
 * @file tray.c
 * @brief System tray — Tor status, network, battery, clock
 * @module wlt-shell/tray
 * @purpose Display system status indicators in the taskbar
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

wlt_shell_error_t
wlt_shell_tray_update(wlt_shell_state_t *state,
                      wlt_shell_tray_id_t tray,
                      const char *status)
{
    if (state == NULL || status == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    if (tray >= WLT_SHELL_TRAY_COUNT)
        return WLT_SHELL_ERR_INVALID_ARG;

    strncpy(state->tray_status[tray], status,
            sizeof(state->tray_status[0]) - 1);
    state->tray_status[tray][sizeof(state->tray_status[0]) - 1] = '\0';

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_tray_get(const wlt_shell_state_t *state,
                   wlt_shell_tray_id_t tray,
                   char *status, size_t max_len)
{
    if (state == NULL || status == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    if (tray >= WLT_SHELL_TRAY_COUNT)
        return WLT_SHELL_ERR_INVALID_ARG;

    strncpy(status, state->tray_status[tray], max_len - 1);
    status[max_len - 1] = '\0';

    return WLT_SHELL_OK;
}

/* Internal: Update clock from system time */
void
wlt_shell_tray_update_clock(wlt_shell_state_t *state)
{
    if (state == NULL)
        return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(state->tray_status[WLT_SHELL_TRAY_CLOCK],
             sizeof(state->tray_status[0]),
             "%02d:%02d", tm->tm_hour, tm->tm_min);
}
