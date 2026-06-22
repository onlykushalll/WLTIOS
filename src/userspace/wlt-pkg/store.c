/**
 * @file store.c
 * @brief Package store management
 * @module wlt-pkg/store
 * @purpose Manage /var/wlt/store/ directory with hashed package paths
 * @dependencies: pkg.h
 * @spec: docs/specs/wlt-pkg.md
 */

#include "pkg.h"
#include <errno.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <errno.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

wlt_pkg_error_t
wlt_pkg_store_init(void)
{
    /* Create store directory structure */
    mkdir("/var/wlt", 0755);
    mkdir("/var/wlt/store", 0755);
    mkdir("/var/wlt/db", 0755);
    mkdir("/var/wlt/cache", 0755);
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_path(const char *name, const char *version,
                   const char *hash, char *path, size_t max_len)
{
    if (name == NULL || version == NULL || hash == NULL || path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    snprintf(path, max_len, "%s/wlt-%s-%s-%s",
             WLT_PKG_STORE_DIR, hash, name, version);

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_add(const char *store_path)
{
    if (store_path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Create the store directory */
    if (mkdir(store_path, 0755) != 0)
    {
        if (errno == EEXIST)
            return WLT_PKG_ERR_EXISTS;
        return WLT_PKG_ERR_IO;
    }

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_remove(const char *store_path)
{
    if (store_path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Recursive removal of store directory */
    if (rmdir(store_path) != 0)
        return WLT_PKG_ERR_IO;

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_list(char store_paths[][WLT_PKG_MAX_PATH], size_t *count,
                   size_t max_count)
{
    if (store_paths == NULL || count == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    *count = 0;
    DIR *d = opendir(WLT_PKG_STORE_DIR);
    if (d == NULL)
        return WLT_PKG_OK;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && *count < max_count)
    {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(store_paths[*count], WLT_PKG_MAX_PATH, "%s/%s",
                 WLT_PKG_STORE_DIR, entry->d_name);
        (*count)++;
    }

    closedir(d);
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_create_symlinks(const char *store_path)
{
    if (store_path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Scan store_path/usr/bin/, create symlinks in /usr/bin/ */
    /* TODO(wlt-pkg): Scan store_path/usr/lib/, create symlinks in /usr/lib/ */
    /* For now, stub */
    (void)store_path;
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_remove_symlinks(const char *store_path)
{
    if (store_path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Remove all symlinks pointing to this store path */
    (void)store_path;
    return WLT_PKG_OK;
}
