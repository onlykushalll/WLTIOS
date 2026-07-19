/**
 * @file dependency.c
 * @brief Dependency resolution — topological sort + conflict detection
 * @module wlt-pkg/dependency
 * @purpose Resolve package dependencies before installation
 * @dependencies: pkg.h
 * @spec: docs/specs/wlt-pkg.md
 *
 * Uses the real package database (wlt_pkg_list) instead of the
 * previous g_db_count_hack() that always returned 0.
 */

#include "pkg.h"
#include <string.h>
#include <stdio.h>

/* ─── Database access ───
 * The database is owned by pkg.c (static g_db). We access it via
 * wlt_pkg_list(), which returns a copy. This avoids circular deps
 * and keeps dependency.c independent of the db storage format. */

wlt_pkg_error_t
wlt_pkg_resolve_dependencies(const char *package_name,
                              char resolved[][WLT_PKG_MAX_NAME],
                              size_t *count, size_t max_count)
{
    if (package_name == NULL || resolved == NULL || count == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    *count = 0;

    /* Get the real database */
    wlt_pkg_database_t db;
    wlt_pkg_error_t err = wlt_pkg_list(&db);
    if (err != WLT_PKG_OK)
        return err;

    /* Find the package in the database */
    wlt_pkg_package_t *pkg = NULL;
    for (size_t i = 0; i < db.count; i++)
    {
        if (strcmp(db.packages[i].name, package_name) == 0)
        {
            pkg = &db.packages[i];
            break;
        }
    }

    /* Add dependencies first (if any) */
    if (pkg)
    {
        for (size_t i = 0; i < pkg->dep_count && *count < max_count; i++)
        {
            /* Check if dependency is already installed */
            bool dep_installed = false;
            for (size_t j = 0; j < db.count; j++)
            {
                if (strcmp(db.packages[j].name, pkg->dependencies[i]) == 0 &&
                    db.packages[j].installed)
                {
                    dep_installed = true;
                    break;
                }
            }
            if (!dep_installed)
            {
                strncpy(resolved[*count], pkg->dependencies[i],
                        WLT_PKG_MAX_NAME - 1);
                resolved[*count][WLT_PKG_MAX_NAME - 1] = '\0';
                (*count)++;
            }
        }
    }

    /* Add the package itself */
    if (*count < max_count)
    {
        strncpy(resolved[*count], package_name, WLT_PKG_MAX_NAME - 1);
        resolved[*count][WLT_PKG_MAX_NAME - 1] = '\0';
        (*count)++;
    }

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_check_conflicts(const char *package_name,
                         char *conflict_name, size_t max_len)
{
    if (package_name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Get the real database */
    wlt_pkg_database_t db;
    wlt_pkg_error_t err = wlt_pkg_list(&db);
    if (err != WLT_PKG_OK)
        return err;

    /* Check if a package with the same name is already installed */
    for (size_t i = 0; i < db.count; i++)
    {
        if (strcmp(db.packages[i].name, package_name) == 0 &&
            db.packages[i].installed)
        {
            if (conflict_name && max_len > 0)
                strncpy(conflict_name, package_name, max_len - 1);
            return WLT_PKG_ERR_EXISTS;
        }
    }

    if (conflict_name && max_len > 0)
        conflict_name[0] = '\0';
    return WLT_PKG_OK;
}

bool
wlt_pkg_is_installed(const char *name)
{
    if (name == NULL)
        return false;

    wlt_pkg_database_t db;
    if (wlt_pkg_list(&db) != WLT_PKG_OK)
        return false;

    for (size_t i = 0; i < db.count; i++)
    {
        if (strcmp(db.packages[i].name, name) == 0)
            return db.packages[i].installed;
    }
    return false;
}
