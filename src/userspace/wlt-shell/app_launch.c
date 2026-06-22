/**
 * @file app_launch.c
 * @brief App launching — read .desktop files, launch with sandbox
 * @module wlt-shell/app_launch
 * @purpose Launch applications with proper sandboxing
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

/* Check if a process is still running */
static bool
is_pid_alive(uint32_t pid)
{
    if (pid == 0)
        return false;
    return kill((pid_t)pid, 0) == 0;
}

/* Find app index by name */
static int
find_app_index(wlt_shell_state_t *state, const char *name)
{
    for (size_t i = 0; i < state->app_count; i++)
    {
        if (strcmp(state->apps[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

/* Launch an app with sandboxing (via wlt-sandbox) */
wlt_shell_error_t
wlt_shell_app_launch(wlt_shell_state_t *state, const char *app_name)
{
    if (state == NULL || app_name == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    int idx = find_app_index(state, app_name);
    if (idx < 0)
        return WLT_SHELL_ERR_NOT_FOUND;

    wlt_shell_app_t *app = &state->apps[idx];

    /* Check if already running */
    if (app->is_running && is_pid_alive(app->pid))
    {
        /* Focus the existing window */
        /* TODO(wlt-shell): Send focus request to compositor */
        return WLT_SHELL_OK;
    }

    /* Fork + exec */
    pid_t pid = fork();
    if (pid < 0)
    {
        return WLT_SHELL_ERR_IO;
    }

    if (pid == 0)
    {
        /* Child process */
        /* TODO(wlt-shell): Set up sandbox via wlt-sandbox before exec */
        /* For now, direct exec */

        /* Parse exec path for arguments (simple space-split) */
        char exec_copy[WLT_SHELL_MAX_PATH];
        strncpy(exec_copy, app->exec_path, WLT_SHELL_MAX_PATH - 1);
        exec_copy[WLT_SHELL_MAX_PATH - 1] = '\0';

        /* Remove %U, %f, etc. from exec (desktop file field codes) */
        char *pct = strchr(exec_copy, '%');
        if (pct) *pct = '\0';

        /* Simple exec — no args parsing for now */
        char *args[] = {"/bin/sh", "-c", exec_copy, NULL};
        execv("/bin/sh", args);
        _exit(127);
    }

    /* Parent */
    app->is_running = true;
    app->pid = (uint32_t)pid;

    return WLT_SHELL_OK;
}

/* Check if any apps have exited and update state */
void
wlt_shell_app_check_exits(wlt_shell_state_t *state)
{
    if (state == NULL)
        return;

    for (size_t i = 0; i < state->app_count; i++)
    {
        if (state->apps[i].is_running && !is_pid_alive(state->apps[i].pid))
        {
            state->apps[i].is_running = false;
            state->apps[i].pid = 0;
        }
    }
}
