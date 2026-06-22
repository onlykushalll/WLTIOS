/**
 * @file taskbar.c
 * @brief Taskbar — running apps, app switcher, system tray area
 * @module wlt-shell/taskbar
 * @purpose Display and manage running applications in the taskbar
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>

wlt_shell_error_t
wlt_shell_taskbar_update_apps(wlt_shell_state_t *state)
{
    if (state == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    /* TODO(wlt-shell): Query WLT-Compositor for running windows */
    /* TODO(wlt-shell): Update app list with window titles + PIDs */
    /* For now, just mark apps as not running */
    for (size_t i = 0; i < state->app_count; i++)
    {
        /* Real implementation: check if PID is still alive */
        /* state->apps[i].is_running = (kill(state->apps[i].pid, 0) == 0); */
    }

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_taskbar_switch_app(wlt_shell_state_t *state, const char *app_name)
{
    if (state == NULL || app_name == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    /* Find the app */
    for (size_t i = 0; i < state->app_count; i++)
    {
        if (strcmp(state->apps[i].name, app_name) == 0)
        {
            /* TODO(wlt-shell): Send focus request to compositor */
            /* wlt_compositor_focus_window(state->apps[i].pid); */
            return WLT_SHELL_OK;
        }
    }

    return WLT_SHELL_ERR_NOT_FOUND;
}
