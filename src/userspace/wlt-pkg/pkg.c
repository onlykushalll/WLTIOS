/**
 * @file pkg.c
 * @brief Main package manager — transaction, database, CLI
 * @module wlt-pkg
 * @purpose Package install/remove/update/rollback orchestration
 * @dependencies: pkg.h
 * @tested_by: tests/userspace/pkg_test.c
 * @spec: docs/specs/wlt-pkg.md
 */

#include "pkg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static wlt_pkg_database_t g_db;

wlt_pkg_error_t
wlt_pkg_init(void)
{
    memset(&g_db, 0, sizeof(g_db));

    /* Ensure store directory exists */
    mkdir("/var/wlt", 0755);
    mkdir("/var/wlt/store", 0755);
    mkdir("/var/wlt/db", 0755);

    /* Load database */
    wlt_pkg_error_t err = wlt_pkg_list(&g_db);
    if (err != WLT_PKG_OK && err != WLT_PKG_ERR_NOT_FOUND)
        return err;

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_shutdown(void)
{
    memset(&g_db, 0, sizeof(g_db));
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_install(const char *name)
{
    if (name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Check if already installed */
    for (size_t i = 0; i < g_db.count; i++)
    {
        if (strcmp(g_db.packages[i].name, name) == 0 && g_db.packages[i].installed)
            return WLT_PKG_ERR_EXISTS;
    }

    /* TODO(wlt-pkg): Full install flow:
     *   1. Resolve dependencies (wlt_pkg_resolve_deps)
     *   2. Download package via Tor (wlt_pkg_download)
     *   3. Verify signature (wlt_pkg_verify)
     *   4. Start transaction (wlt_pkg_transaction_begin)
     *   5. Extract to store (wlt_pkg_store_add)
     *   6. Create symlinks
     *   7. Update database
     *   8. Commit transaction (wlt_pkg_transaction_commit)
     *   On failure: rollback (wlt_pkg_transaction_rollback)
     */

    printf("Installing %s...\n", name);

    /* For now, just add to database as installed */
    if (g_db.count < 256)
    {
        wlt_pkg_package_t *pkg = &g_db.packages[g_db.count];
        memset(pkg, 0, sizeof(*pkg));
        strncpy(pkg->name, name, WLT_PKG_MAX_NAME - 1);
        strncpy(pkg->version, "1.0.0", WLT_PKG_MAX_VERSION - 1);
        pkg->installed = true;
        g_db.count++;
    }

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_remove(const char *name)
{
    if (name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    for (size_t i = 0; i < g_db.count; i++)
    {
        if (strcmp(g_db.packages[i].name, name) == 0)
        {
            if (!g_db.packages[i].installed)
                return WLT_PKG_ERR_NOT_FOUND;

            /* TODO(wlt-pkg): Full remove flow:
             *   1. Start transaction
             *   2. Remove symlinks
             *   3. Remove from store
             *   4. Update database
             *   5. Commit
             */

            g_db.packages[i].installed = false;
            return WLT_PKG_OK;
        }
    }

    return WLT_PKG_ERR_NOT_FOUND;
}

wlt_pkg_error_t
wlt_pkg_update(const char *name)
{
    if (name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Check if installed */
    bool found = false;
    for (size_t i = 0; i < g_db.count; i++)
    {
        if (strcmp(g_db.packages[i].name, name) == 0 && g_db.packages[i].installed)
        {
            found = true;
            break;
        }
    }

    if (!found)
        return WLT_PKG_ERR_NOT_FOUND;

    /* TODO(wlt-pkg): Check for newer version, download, verify, atomic swap */
    printf("Checking for updates to %s...\n", name);

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_update_all(void)
{
    int updated = 0;
    for (size_t i = 0; i < g_db.count; i++)
    {
        if (g_db.packages[i].installed)
        {
            wlt_pkg_error_t err = wlt_pkg_update(g_db.packages[i].name);
            if (err == WLT_PKG_OK)
                updated++;
        }
    }

    printf("Updated %d packages.\n", updated);
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_rollback(const char *name)
{
    if (name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Find previous version in store, swap symlinks */
    printf("Rolling back %s...\n", name);

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_list(wlt_pkg_database_t *db)
{
    if (db == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Read from /var/wlt/db/packages.json */
    /* For now, copy from global db */
    memcpy(db, &g_db, sizeof(*db));

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_search(const char *query, wlt_pkg_database_t *results)
{
    if (query == NULL || results == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    memset(results, 0, sizeof(*results));

    for (size_t i = 0; i < g_db.count; i++)
    {
        if (strstr(g_db.packages[i].name, query) != NULL)
        {
            if (results->count < 256)
            {
                results->packages[results->count] = g_db.packages[i];
                results->count++;
            }
        }
    }

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_info(const char *name, wlt_pkg_package_t *pkg)
{
    if (name == NULL || pkg == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    for (size_t i = 0; i < g_db.count; i++)
    {
        if (strcmp(g_db.packages[i].name, name) == 0)
        {
            memcpy(pkg, &g_db.packages[i], sizeof(*pkg));
            return WLT_PKG_OK;
        }
    }

    return WLT_PKG_ERR_NOT_FOUND;
}

wlt_pkg_error_t
wlt_pkg_check_updates(char *updates, size_t max_count)
{
    if (updates == NULL || max_count == 0)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Query repository via Tor for available updates */
    updates[0] = '\0';

    return WLT_PKG_OK;
}

const char *
wlt_pkg_error_str(wlt_pkg_error_t err)
{
    switch (err)
    {
        case WLT_PKG_OK:               return "OK";
        case WLT_PKG_ERR_INVALID_ARG:  return "Invalid argument";
        case WLT_PKG_ERR_NOT_FOUND:    return "Package not found";
        case WLT_PKG_ERR_IO:           return "I/O error";
        case WLT_PKG_ERR_NOMEM:        return "Out of memory";
        case WLT_PKG_ERR_SIGNATURE:    return "Signature verification failed";
        case WLT_PKG_ERR_EXISTS:       return "Already installed";
        case WLT_PKG_ERR_CONFLICT:     return "Dependency conflict";
        case WLT_PKG_ERR_NETWORK:      return "Network error";
        case WLT_PKG_ERR_DISK_FULL:    return "Disk full";
        default:                       return "Unknown error";
    }
}
