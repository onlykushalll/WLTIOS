/**
 * @file dependency.c
 * @brief Dependency resolution — topological sort + conflict detection
 * @module wlt-pkg/dependency
 * @purpose Resolve package dependencies before installation
 * @dependencies: pkg.h
 * @spec: docs/specs/wlt-pkg.md
 */

#include "pkg.h"
#include <string.h>
#include <stdio.h>

/* Forward declaration — defined at end of file */
static size_t g_db_count_hack(void);

wlt_pkg_error_t
wlt_pkg_resolve_dependencies(const char *package_name,
                              char resolved[][WLT_PKG_MAX_NAME],
                              size_t *count, size_t max_count)
{
    if (package_name == NULL || resolved == NULL || count == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    *count = 0;

    /* Find the package in the database */
    wlt_pkg_package_t *pkg = NULL;
 (void)pkg; /* TODO: real impl will use this */
    for (size_t i = 0; i < g_db_count_hack(); i++)
    {
        /* TODO(wlt-pkg): Real implementation — traverse dependency graph */
    }

    /* For now, just return the package itself */
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

    /* TODO(wlt-pkg): Check if package conflicts with installed packages */
    /* For now, no conflicts */
    if (conflict_name && max_len > 0)
        conflict_name[0] = '\0';

    (void)package_name;
    return WLT_PKG_OK;
}

bool
wlt_pkg_is_installed(const char *name)
{
    if (name == NULL)
        return false;

    /* TODO(wlt-pkg): Check database */
    (void)name;
    return false;
}

/* Hack to avoid circular dependency — real implementation uses the database */
static size_t g_db_count_hack(void) { return 0; }
