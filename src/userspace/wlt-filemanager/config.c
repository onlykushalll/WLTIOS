/**
 * @file config.c
 * @brief Config file parsing for WLT-FileManager
 * @module wlt-filemanager/config
 * @purpose Parse INI-style config for file manager preferences
 * @dependencies: filemanager.h
 * @tested_by: tests/userspace/fm_config_test.c
 * @spec: docs/specs/wlt-filemanager.md
 */

#include "filemanager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    bool show_hidden;
    bool confirm_delete;
    bool secure_delete_default;
    int shred_passes;
    char default_path[WLT_FM_MAX_PATH];
    int sort_by;        /* 0=name, 1=size, 2=date */
    bool sort_descending;
} wlt_fm_config_t;

/* ─── Internal: trim() ───────────────────────────────── */

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

/* ─── Internal: parse_kv() ───────────────────────────── */

static int
parse_kv(const char *line, char *key, int key_max, char *val, int val_max)
{
    const char *eq = strchr(line, '=');
    if (eq == NULL)
        return -1;

    int klen = eq - line;
    if (klen >= key_max)
        klen = key_max - 1;
    strncpy(key, line, klen);
    key[klen] = '\0';
    trim(key);

    const char *vstart = eq + 1;
    int vlen = strlen(vstart);
    if (vlen >= val_max)
        vlen = val_max - 1;
    strncpy(val, vstart, vlen);
    val[vlen] = '\0';
    trim(val);

    return 0;
}

/* ─── Public API ─────────────────────────────────────── */

void
wlt_fm_config_get_default(wlt_fm_config_t *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));
    config->show_hidden = false;
    config->confirm_delete = true;
    config->secure_delete_default = true;
    config->shred_passes = 3;
    strncpy(config->default_path, "/home", WLT_FM_MAX_PATH - 1);
    config->sort_by = 0;  /* name */
    config->sort_descending = false;
}

wlt_filemanager_error_t
wlt_fm_config_load(const char *path, wlt_fm_config_t *config)
{
    if (path == NULL || config == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    wlt_fm_config_get_default(config);

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return WLT_FM_OK;  /* Use defaults if no config file */

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *trimmed = trim(line);

        /* Skip empty + comments */
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        /* Section header */
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

        /* Key = value */
        char key[128], val[256];
        if (parse_kv(trimmed, key, sizeof(key), val, sizeof(val)) == 0)
        {
            if (strcmp(section, "filemanager") == 0)
            {
                if (strcmp(key, "show_hidden") == 0)
                    config->show_hidden = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0);
                else if (strcmp(key, "confirm_delete") == 0)
                    config->confirm_delete = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0);
                else if (strcmp(key, "secure_delete") == 0)
                    config->secure_delete_default = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0);
                else if (strcmp(key, "shred_passes") == 0)
                    config->shred_passes = atoi(val);
                else if (strcmp(key, "default_path") == 0)
                {
                    strncpy(config->default_path, val, WLT_FM_MAX_PATH - 1);
                    config->default_path[WLT_FM_MAX_PATH - 1] = '\0';
                }
            }
            else if (strcmp(section, "sort") == 0)
            {
                if (strcmp(key, "by") == 0)
                {
                    if (strcmp(val, "name") == 0) config->sort_by = 0;
                    else if (strcmp(val, "size") == 0) config->sort_by = 1;
                    else if (strcmp(val, "date") == 0) config->sort_by = 2;
                }
                else if (strcmp(key, "descending") == 0)
                    config->sort_descending = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0);
            }
        }
    }

    fclose(f);
    return WLT_FM_OK;
}
