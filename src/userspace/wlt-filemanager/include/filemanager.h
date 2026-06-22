/**
 * @file filemanager.h
 * @brief Public API for WLT-FileManager (file manager backend)
 * @module wlt-filemanager
 * @purpose Directory browsing, file operations, secure delete
 * @dependencies: operations.h, config.h
 * @tested_by: tests/userspace/filemanager_test.c
 * @spec: docs/specs/wlt-filemanager.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#ifndef WLT_FILEMANAGER_H
#define WLT_FILEMANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_FM_OK = 0,
    WLT_FM_ERR_INVALID_ARG = -1,
    WLT_FM_ERR_NOT_FOUND = -2,
    WLT_FM_ERR_PERMISSION = -3,
    WLT_FM_ERR_IO = -4,
    WLT_FM_ERR_NOMEM = -5,
    WLT_FM_ERR_EXISTS = -6,
    WLT_FM_ERR_NOT_EMPTY = -7,
    WLT_FM_ERR_TOO_LONG = -8,
    WLT_FM_ERR_READONLY = -9,
} wlt_filemanager_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_FM_MAX_PATH 4096
#define WLT_FM_MAX_NAME 256
#define WLT_FM_MAX_ENTRIES 1024
#define WLT_FM_VERSION "1.0.0"

/* ─── Types ──────────────────────────────────────────── */

typedef enum {
    WLT_FM_TYPE_FILE = 0,
    WLT_FM_TYPE_DIRECTORY = 1,
    WLT_FM_TYPE_SYMLINK = 2,
    WLT_FM_TYPE_OTHER = 3,
} wlt_fm_entry_type_t;

typedef struct {
    char name[WLT_FM_MAX_NAME];
    wlt_fm_entry_type_t type;
    uint64_t size;
    uint64_t modified_time;
    bool is_encrypted;
    bool is_hidden;
} wlt_fm_entry_t;

typedef struct {
    wlt_fm_entry_t entries[WLT_FM_MAX_ENTRIES];
    size_t count;
    char current_path[WLT_FM_MAX_PATH];
} wlt_fm_directory_t;

typedef struct {
    char current_path[WLT_FM_MAX_PATH];
    char clipboard_path[WLT_FM_MAX_PATH];
    bool clipboard_is_cut;
    bool show_hidden;
} wlt_fm_state_t;

/* ─── Public API ─────────────────────────────────────── */

/**
 * Initialize file manager state
 */
wlt_filemanager_error_t wlt_fm_init(wlt_fm_state_t *state, const char *start_path);

/**
 * List directory contents
 * @param path      Directory to list
 * @param dir       Output directory listing
 * @param show_hidden  Include hidden files (dotfiles)
 */
wlt_filemanager_error_t wlt_fm_list_directory(const char *path,
                                              wlt_fm_directory_t *dir,
                                              bool show_hidden);

/**
 * Navigate to a path (validates it exists + is a directory)
 */
wlt_filemanager_error_t wlt_fm_navigate(wlt_fm_state_t *state, const char *path);

/**
 * Go up one directory level
 */
wlt_filemanager_error_t wlt_fm_navigate_up(wlt_fm_state_t *state);

/**
 * Get error string
 */
const char *wlt_fm_error_str(wlt_filemanager_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_FILEMANAGER_H */
