/**
 * @file operations.h
 * @brief File operations API (copy, move, delete, mkdir, rmdir, shred)
 * @module wlt-filemanager/operations
 * @purpose All file manipulation operations
 * @dependencies: filemanager.h
 * @tested_by: tests/userspace/operations_test.c
 * @spec: docs/specs/wlt-filemanager.md
 */

#ifndef WLT_FM_OPERATIONS_H
#define WLT_FM_OPERATIONS_H

#include "filemanager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── File Operations ────────────────────────────────── */

/**
 * Copy a file
 * @param src       Source file path
 * @param dst       Destination file path
 * @return WLT_FM_OK on success
 */
wlt_filemanager_error_t wlt_fm_copy_file(const char *src, const char *dst);

/**
 * Copy a directory recursively
 */
wlt_filemanager_error_t wlt_fm_copy_directory(const char *src, const char *dst);

/**
 * Move (rename) a file or directory
 */
wlt_filemanager_error_t wlt_fm_move(const char *src, const char *dst);

/**
 * Delete a file
 */
wlt_filemanager_error_t wlt_fm_delete_file(const char *path);

/**
 * Delete a directory recursively
 */
wlt_filemanager_error_t wlt_fm_delete_directory(const char *path);

/**
 * Create a directory
 */
wlt_filemanager_error_t wlt_fm_mkdir(const char *path);

/**
 * Secure delete (shred) a file — 3-pass overwrite + unlink
 * @param path   File to shred
 * @param passes Number of overwrite passes (default 3)
 */
wlt_filemanager_error_t wlt_fm_secure_delete(const char *path, int passes);

/**
 * Get file size
 */
wlt_filemanager_error_t wlt_fm_get_size(const char *path, uint64_t *size);

/**
 * Check if path exists
 */
bool wlt_fm_path_exists(const char *path);

/**
 * Check if path is a directory
 */
bool wlt_fm_is_directory(const char *path);

/**
 * Check if path is a file
 */
bool wlt_fm_is_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WLT_FM_OPERATIONS_H */
