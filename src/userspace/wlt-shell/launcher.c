/**
 * @file launcher.c
 * @brief App launcher — search, indexed apps, launch on enter
 * @module wlt-shell/launcher
 * @purpose Search and launch applications
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

static bool
has_desktop_extension(const char *name)
{
    size_t len = strlen(name);
    return len > 8 && strcmp(name + len - 8, ".desktop") == 0;
}

static void
parse_desktop_file(const char *path, wlt_shell_app_t *app)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return;

    char line[512];
    bool in_desktop_entry = false;

    while (fgets(line, sizeof(line), f) != NULL)
    {
        /* Remove newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        /* Section header */
        if (line[0] == '[')
        {
            in_desktop_entry = (strcmp(line, "[Desktop Entry]") == 0);
            continue;
        }

        if (!in_desktop_entry)
            continue;

        /* Parse key=value */
        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;

        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strcmp(key, "Name") == 0)
        {
            strncpy(app->name, val, WLT_SHELL_MAX_NAME - 1);
            app->name[WLT_SHELL_MAX_NAME - 1] = '\0';
        }
        else if (strcmp(key, "Exec") == 0)
        {
            strncpy(app->exec_path, val, WLT_SHELL_MAX_PATH - 1);
            app->exec_path[WLT_SHELL_MAX_PATH - 1] = '\0';
        }
        else if (strcmp(key, "Icon") == 0)
        {
            strncpy(app->icon_path, val, WLT_SHELL_MAX_PATH - 1);
            app->icon_path[WLT_SHELL_MAX_PATH - 1] = '\0';
        }
    }

    fclose(f);
}

wlt_shell_error_t
wlt_shell_launcher_toggle(wlt_shell_state_t *state)
{
    if (state == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    state->launcher_visible = !state->launcher_visible;
    if (state->launcher_visible)
    {
        state->launcher_query[0] = '\0';

        /* Index apps from /usr/share/applications/ */
        DIR *d = opendir("/usr/share/applications");
        if (d == NULL)
            return WLT_SHELL_OK;

        state->app_count = 0;
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL &&
               state->app_count < WLT_SHELL_MAX_APPS)
        {
            if (!has_desktop_extension(entry->d_name))
                continue;

            char path[WLT_SHELL_MAX_PATH];
            snprintf(path, sizeof(path), "/usr/share/applications/%s",
                     entry->d_name);

            wlt_shell_app_t *app = &state->apps[state->app_count];
            memset(app, 0, sizeof(*app));
            parse_desktop_file(path, app);
            app->is_running = false;
            app->pid = 0;

            if (app->name[0] != '\0')
                state->app_count++;
        }
        closedir(d);
    }

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_launcher_search(wlt_shell_state_t *state, const char *query)
{
    if (state == NULL || query == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    strncpy(state->launcher_query, query, WLT_SHELL_MAX_NAME - 1);
    state->launcher_query[WLT_SHELL_MAX_NAME - 1] = '\0';

    /* TODO(wlt-shell): Filter app list by query (case-insensitive substring match) */
    /* Real implementation will reorder apps[] to show matches first */

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_launcher_launch(wlt_shell_state_t *state, const char *app_name)
{
    if (state == NULL || app_name == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    /* Find the app */
    for (size_t i = 0; i < state->app_count; i++)
    {
        if (strcmp(state->apps[i].name, app_name) == 0)
        {
            /* TODO(wlt-shell): Launch via WLT-Sandbox (fork + sandbox setup + exec) */
            /* For now, just fork + exec */
            pid_t pid = fork();
            if (pid == 0)
            {
                /* Child */
                execl(state->apps[i].exec_path, app_name, NULL);
                _exit(127);
            }
            if (pid > 0)
            {
                state->apps[i].is_running = true;
                state->apps[i].pid = (uint32_t)pid;
                state->launcher_visible = false;
                return WLT_SHELL_OK;
            }
            return WLT_SHELL_ERR_IO;
        }
    }

    return WLT_SHELL_ERR_NOT_FOUND;
}
