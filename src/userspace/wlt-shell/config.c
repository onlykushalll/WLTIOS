/**
 * @file config.c
 * @brief Config parsing for WLT-Shell
 * @module wlt-shell/config
 * @purpose Parse INI-style config for shell preferences
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct {
    int taskbar_position;       /* 0=bottom, 1=top, 2=left, 3=right */
    int taskbar_height;
    bool show_clock;
    bool show_battery;
    bool show_tor_status;
    bool show_network;
    int notification_timeout_ms;
    char launcher_default[WLT_SHELL_MAX_NAME];
} wlt_shell_config_t;

static char *
trim(char *str)
{
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != str)
        memmove(str, start, strlen(start) + 1);
    if (*str)
    {
        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
            *end-- = '\0';
    }
    return str;
}

void
wlt_shell_config_get_default(wlt_shell_config_t *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));
    config->taskbar_position = 0;  /* bottom */
    config->taskbar_height = 32;
    config->show_clock = true;
    config->show_battery = true;
    config->show_tor_status = true;
    config->show_network = true;
    config->notification_timeout_ms = WLT_SHELL_NOTIFICATION_TIMEOUT_MS;
    strncpy(config->launcher_default, "terminal", WLT_SHELL_MAX_NAME - 1);
}

wlt_shell_error_t
wlt_shell_config_load(const char *path, wlt_shell_config_t *config)
{
    if (path == NULL || config == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    wlt_shell_config_get_default(config);

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return WLT_SHELL_OK;  /* Use defaults */

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *trimmed = trim(line);
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        if (*trimmed == '[')
        {
            char *end = strchr(trimmed, ']');
            if (end)
            {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
            }
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (eq == NULL)
            continue;

        *eq = '\0';
        char *key = trim(trimmed);
        char *val = trim(eq + 1);

        if (strcmp(section, "taskbar") == 0)
        {
            if (strcmp(key, "position") == 0)
                config->taskbar_position = atoi(val);
            else if (strcmp(key, "height") == 0)
                config->taskbar_height = atoi(val);
            else if (strcmp(key, "show_clock") == 0)
                config->show_clock = (strcmp(val, "yes") == 0);
            else if (strcmp(key, "show_battery") == 0)
                config->show_battery = (strcmp(val, "yes") == 0);
            else if (strcmp(key, "show_tor") == 0)
                config->show_tor_status = (strcmp(val, "yes") == 0);
            else if (strcmp(key, "show_network") == 0)
                config->show_network = (strcmp(val, "yes") == 0);
        }
        else if (strcmp(section, "notifications") == 0)
        {
            if (strcmp(key, "timeout_ms") == 0)
                config->notification_timeout_ms = atoi(val);
        }
        else if (strcmp(section, "launcher") == 0)
        {
            if (strcmp(key, "default_app") == 0)
            {
                strncpy(config->launcher_default, val, WLT_SHELL_MAX_NAME - 1);
                config->launcher_default[WLT_SHELL_MAX_NAME - 1] = '\0';
            }
        }
    }

    fclose(f);
    return WLT_SHELL_OK;
}
