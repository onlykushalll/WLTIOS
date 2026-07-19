/**
 * @file store.c
 * @brief Package store management
 * @module wlt-pkg/store
 * @purpose Manage /var/wlt/store/ directory with hashed package paths
 * @dependencies: pkg.h
 * @spec: docs/specs/wlt-pkg.md
 *
 * Implements REAL symlink management and recursive store removal.
 */

#define _GNU_SOURCE
#include "pkg.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ftw.h>

/* ─── Recursive removal via nftw ─── */

static int
rm_cb(const char *path, const struct stat *sb,
      int typeflag, struct FTW *ftwbuf)
{
    (void)sb; (void)typeflag; (void)ftwbuf;
    return remove(path);
}

wlt_pkg_error_t
wlt_pkg_store_init(void)
{
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

    /* Recursive removal using nftw */
    if (nftw(store_path, rm_cb, 64, FTW_DEPTH | FTW_PHYS) != 0)
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

    /* Scan store_path/usr/bin/ and create symlinks in /usr/bin/ */
    char bin_dir[WLT_PKG_MAX_PATH];
    snprintf(bin_dir, sizeof(bin_dir), "%s/usr/bin", store_path);

    DIR *d = opendir(bin_dir);
    if (d == NULL)
        return WLT_PKG_OK;  /* No usr/bin in this package — not an error */

    struct dirent *entry;
    int created = 0;
    while ((entry = readdir(d)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        char src[WLT_PKG_MAX_PATH], dst[WLT_PKG_MAX_PATH];
        snprintf(src, sizeof(src), "%s/usr/bin/%s", store_path, entry->d_name);
        snprintf(dst, sizeof(dst), "/usr/bin/%s", entry->d_name);

        /* Remove existing, then create symlink */
        unlink(dst);
        if (symlink(src, dst) == 0)
            created++;
    }
    closedir(d);

    printf("[wlt-pkg/store] created %d symlinks from %s\n", created, bin_dir);
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_store_remove_symlinks(const char *store_path)
{
    if (store_path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Scan /usr/bin for symlinks pointing to this store path */
    DIR *d = opendir("/usr/bin");
    if (d == NULL)
        return WLT_PKG_OK;

    struct dirent *entry;
    int removed = 0;
    while ((entry = readdir(d)) != NULL)
    {
        char link_path[WLT_PKG_MAX_PATH];
        snprintf(link_path, sizeof(link_path), "/usr/bin/%s", entry->d_name);

        char target[WLT_PKG_MAX_PATH];
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if (len > 0)
        {
            target[len] = '\0';
            if (strstr(target, store_path))
            {
                unlink(link_path);
                removed++;
            }
        }
    }
    closedir(d);

    printf("[wlt-pkg/store] removed %d symlinks pointing to %s\n",
           removed, store_path);
    return WLT_PKG_OK;
}
