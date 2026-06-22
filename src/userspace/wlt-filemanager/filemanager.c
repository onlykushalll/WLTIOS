/**
 * @file filemanager.c
 * @brief Main file manager logic — directory listing + navigation
 * @module wlt-filemanager
 * @purpose Browse directories, list entries, navigate filesystem
 * @dependencies: filemanager.h, operations.h
 * @tested_by: tests/userspace/filemanager_test.c
 * @spec: docs/specs/wlt-filemanager.md
 */

#include "filemanager.h"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "operations.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

/* ─── Internal: Get entry type from stat ─────────────── */

static wlt_fm_entry_type_t
get_entry_type(const struct stat *st)
{
    if (S_ISREG(st->st_mode))
        return WLT_FM_TYPE_FILE;
    if (S_ISDIR(st->st_mode))
        return WLT_FM_TYPE_DIRECTORY;
    if (S_ISLNK(st->st_mode))
        return WLT_FM_TYPE_SYMLINK;
    return WLT_FM_TYPE_OTHER;
}

/* ─── Internal: Check if filename is hidden ──────────── */

static bool
is_hidden(const char *name)
{
    return name[0] == '.';
}

/* ─── Public API ─────────────────────────────────────── */

wlt_filemanager_error_t
wlt_fm_init(wlt_fm_state_t *state, const char *start_path)
{
    if (state == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));

    if (start_path == NULL)
        start_path = "/";

    if (!wlt_fm_is_directory(start_path))
        return WLT_FM_ERR_NOT_FOUND;

    strncpy(state->current_path, start_path, WLT_FM_MAX_PATH - 1);
    state->current_path[WLT_FM_MAX_PATH - 1] = '\0';
    state->clipboard_path[0] = '\0';
    state->clipboard_is_cut = false;
    state->show_hidden = false;

    return WLT_FM_OK;
}

wlt_filemanager_error_t
wlt_fm_list_directory(const char *path,
                      wlt_fm_directory_t *dir,
                      bool show_hidden)
{
    if (path == NULL || dir == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    DIR *d = opendir(path);
    if (d == NULL)
    {
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        if (errno == ENOENT)
            return WLT_FM_ERR_NOT_FOUND;
        return WLT_FM_ERR_IO;
    }

    memset(dir, 0, sizeof(*dir));
    strncpy(dir->current_path, path, WLT_FM_MAX_PATH - 1);

    struct dirent *entry;
    dir->count = 0;

    while ((entry = readdir(d)) != NULL && dir->count < WLT_FM_MAX_ENTRIES)
    {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* Skip hidden files if not showing them */
        if (!show_hidden && is_hidden(entry->d_name))
            continue;

        wlt_fm_entry_t *e = &dir->entries[dir->count];
        strncpy(e->name, entry->d_name, WLT_FM_MAX_NAME - 1);
        e->name[WLT_FM_MAX_NAME - 1] = '\0';
        e->is_hidden = is_hidden(entry->d_name);

        /* Get file stats */
        char full_path[WLT_FM_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == 0)
        {
            e->type = get_entry_type(&st);
            e->size = (uint64_t)st.st_size;
            e->modified_time = (uint64_t)st.st_mtime;
        }
        else
        {
            e->type = WLT_FM_TYPE_OTHER;
            e->size = 0;
            e->modified_time = 0;
        }

        /* Check if encrypted (has .wltenc extension) */
        size_t name_len = strlen(e->name);
        if (name_len >= 7 && strcmp(e->name + name_len - 7, ".wltenc") == 0)
            e->is_encrypted = true;
        else
            e->is_encrypted = false;

        dir->count++;
    }

    closedir(d);
    return WLT_FM_OK;
}

wlt_filemanager_error_t
wlt_fm_navigate(wlt_fm_state_t *state, const char *path)
{
    if (state == NULL || path == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    /* Resolve path (handle relative paths) */
    char resolved[WLT_FM_MAX_PATH];

    if (path[0] == '/')
    {
        /* Absolute path */
        strncpy(resolved, path, WLT_FM_MAX_PATH - 1);
    }
    else if (strcmp(path, "..") == 0)
    {
        /* Go up */
        return wlt_fm_navigate_up(state);
    }
    else
    {
        /* Relative path */
        snprintf(resolved, sizeof(resolved), "%s/%s", state->current_path, path);
    }

    resolved[WLT_FM_MAX_PATH - 1] = '\0';

    /* Check it's a directory */
    if (!wlt_fm_is_directory(resolved))
        return WLT_FM_ERR_NOT_FOUND;

    strncpy(state->current_path, resolved, WLT_FM_MAX_PATH - 1);
    state->current_path[WLT_FM_MAX_PATH - 1] = '\0';

    return WLT_FM_OK;
}

wlt_filemanager_error_t
wlt_fm_navigate_up(wlt_fm_state_t *state)
{
    if (state == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    char *last_slash = strrchr(state->current_path, '/');
    if (last_slash == NULL || last_slash == state->current_path)
    {
        /* Already at root */
        strncpy(state->current_path, "/", WLT_FM_MAX_PATH - 1);
    }
    else
    {
        *last_slash = '\0';
        if (state->current_path[0] == '\0')
            strncpy(state->current_path, "/", WLT_FM_MAX_PATH - 1);
    }

    return WLT_FM_OK;
}

const char *
wlt_fm_error_str(wlt_filemanager_error_t err)
{
    switch (err)
    {
        case WLT_FM_OK:               return "OK";
        case WLT_FM_ERR_INVALID_ARG:  return "Invalid argument";
        case WLT_FM_ERR_NOT_FOUND:    return "Not found";
        case WLT_FM_ERR_PERMISSION:   return "Permission denied";
        case WLT_FM_ERR_IO:           return "I/O error";
        case WLT_FM_ERR_NOMEM:        return "Out of memory";
        case WLT_FM_ERR_EXISTS:       return "Already exists";
        case WLT_FM_ERR_NOT_EMPTY:    return "Directory not empty";
        case WLT_FM_ERR_TOO_LONG:     return "Path too long";
        case WLT_FM_ERR_READONLY:     return "Read-only filesystem";
        default:                      return "Unknown error";
    }
}
