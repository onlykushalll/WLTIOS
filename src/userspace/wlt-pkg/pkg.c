/**
 * @file pkg.c
 * @brief Main package manager — transaction, database, CLI
 * @module wlt-pkg
 * @purpose Package install/remove/update/rollback orchestration
 * @dependencies: pkg.h
 * @tested_by: tests/userspace/pkg_test.c
 * @spec: docs/specs/wlt-pkg.md
 *
 * Implements REAL install/remove: extracts package tarball to store,
 * creates symlinks in /usr/bin, persists database to disk.
 */

#include "pkg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Path overrides (env vars for testing without root) */
static const char *
store_dir(void) {
    const char *e = getenv("WLT_PKG_STORE_DIR");
    return e ? e : WLT_PKG_STORE_DIR;
}
static const char *
db_file(void) {
    const char *e = getenv("WLT_PKG_DB_FILE");
    return e ? e : WLT_PKG_DB_FILE;
}
static const char *
cache_dir(void) {
    static char buf[256];
    const char *e = getenv("WLT_PKG_CACHE_DIR");
    if (e) return e;
    snprintf(buf, sizeof(buf), "%s/../cache", store_dir());
    return buf;
}
static const char *
bin_dir(void) {
    const char *e = getenv("WLT_PKG_BIN_DIR");
    return e ? e : "/usr/bin";
}
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

static wlt_pkg_database_t g_db;

/* ─── Database persistence (simple line-based format) ─── */

static wlt_pkg_error_t
db_save(void)
{
    FILE *f = fopen(db_file(), "w");
    if (!f) return WLT_PKG_ERR_IO;

    for (size_t i = 0; i < g_db.count; i++)
    {
        wlt_pkg_package_t *p = &g_db.packages[i];
        fprintf(f, "name=%s\n", p->name);
        fprintf(f, "version=%s\n", p->version);
        fprintf(f, "installed=%d\n", p->installed ? 1 : 0);
        fprintf(f, "store_hash=%s\n", p->store_hash[0] ? p->store_hash : "none");
        fprintf(f, "dep_count=%zu\n", p->dep_count);
        for (size_t j = 0; j < p->dep_count; j++)
            fprintf(f, "dep=%s\n", p->dependencies[j]);
        fprintf(f, "---\n");
    }
    fclose(f);
    return WLT_PKG_OK;
}

static wlt_pkg_error_t
db_load(void)
{
    FILE *f = fopen(db_file(), "r");
    if (!f) return WLT_PKG_ERR_NOT_FOUND;

    g_db.count = 0;
    char line[512];
    wlt_pkg_package_t *cur = NULL;

    while (fgets(line, sizeof(line), f) && g_db.count < 256)
    {
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (strcmp(line, "---") == 0)
        {
            if (cur) g_db.count++;
            cur = NULL;
            continue;
        }

        if (!cur)
        {
            cur = &g_db.packages[g_db.count];
            memset(cur, 0, sizeof(*cur));
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strcmp(key, "name") == 0)
            strncpy(cur->name, val, WLT_PKG_MAX_NAME - 1);
        else if (strcmp(key, "version") == 0)
            strncpy(cur->version, val, WLT_PKG_MAX_VERSION - 1);
        else if (strcmp(key, "installed") == 0)
            cur->installed = (atoi(val) != 0);
        else if (strcmp(key, "store_hash") == 0)
            strncpy(cur->store_hash, val, sizeof(cur->store_hash) - 1);
        else if (strcmp(key, "dep_count") == 0)
            cur->dep_count = (size_t)atoi(val);
        else if (strcmp(key, "dep") == 0 && cur->dep_count < WLT_PKG_MAX_DEPS)
        {
            strncpy(cur->dependencies[cur->dep_count], val, WLT_PKG_MAX_NAME - 1);
            cur->dep_count++;
        }
    }
    /* Last package without trailing --- */
    if (cur && cur->name[0]) g_db.count++;

    fclose(f);
    return WLT_PKG_OK;
}

/* ─── Lifecycle ─── */

wlt_pkg_error_t
wlt_pkg_init(void)
{
    memset(&g_db, 0, sizeof(g_db));
    /* Create store/db/cache dirs (uses env overrides if set) */
    const char *sd = store_dir();
    const char *df = db_file();
    /* Create parent dirs */
    char parent[256];
    snprintf(parent, sizeof(parent), "%s", sd);
    char *slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; mkdir(parent, 0755); }
    mkdir(sd, 0755);
    snprintf(parent, sizeof(parent), "%s", df);
    slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; mkdir(parent, 0755); }
    mkdir(cache_dir(), 0755);
    db_load();  /* Non-fatal if db doesn't exist yet */
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_shutdown(void)
{
    memset(&g_db, 0, sizeof(g_db));
    return WLT_PKG_OK;
}

/* ─── Internal: find package in db ─── */

static wlt_pkg_package_t *
find_package(const char *name)
{
    for (size_t i = 0; i < g_db.count; i++)
    {
        if (strcmp(g_db.packages[i].name, name) == 0)
            return &g_db.packages[i];
    }
    return NULL;
}

/* ─── Install ─── */

wlt_pkg_error_t
wlt_pkg_install(const char *name)
{
    if (name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Check if already installed */
    wlt_pkg_package_t *existing = find_package(name);
    if (existing && existing->installed)
        return WLT_PKG_ERR_EXISTS;

    /* Look for a local package tarball in the cache */
    char pkg_path[WLT_PKG_MAX_PATH];
    snprintf(pkg_path, sizeof(pkg_path), "%s/%s.tar.gz", cache_dir(), name);

    if (access(pkg_path, R_OK) != 0)
    {
        /* No local package — register as installed without extraction
         * (for virtual/meta packages) */
        printf("[wlt-pkg] No tarball at %s — registering %s as virtual\n",
               pkg_path, name);
        if (!existing)
        {
            if (g_db.count >= 256) return WLT_PKG_ERR_NOMEM;
            existing = &g_db.packages[g_db.count];
            memset(existing, 0, sizeof(*existing));
            strncpy(existing->name, name, WLT_PKG_MAX_NAME - 1);
            strncpy(existing->version, "1.0.0", WLT_PKG_MAX_VERSION - 1);
            g_db.count++;
        }
        existing->installed = true;
        db_save();
        return WLT_PKG_OK;
    }

    /* Generate a simple hash from name+timestamp (not crypto, just unique) */
    char hash[65];
    snprintf(hash, sizeof(hash), "%s_%ld", name, (long)time(NULL));

    /* Create store directory */
    char store_path[WLT_PKG_MAX_PATH];
    snprintf(store_path, sizeof(store_path), "%s/wlt-%s-%s-1.0.0",
             store_dir(), hash, name);
    if (mkdir(store_path, 0755) != 0)
    {
        if (errno == EEXIST) return WLT_PKG_ERR_EXISTS;
        return WLT_PKG_ERR_IO;
    }

    /* Extract tarball to store path using system("tar") */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "tar xzf '%s' -C '%s' 2>/dev/null", pkg_path, store_path);
    int rc = system(cmd);
    (void)rc;
    if (rc != 0)
    {
        /* Extraction failed — clean up */
        rmdir(store_path);
        return WLT_PKG_ERR_IO;
    }

    /* Create symlinks for binaries in store_path/usr/bin/ */
    char bd[WLT_PKG_MAX_PATH];
    snprintf(bd, sizeof(bd), "%s/usr/bin", store_path);
    DIR *d = opendir(bd);
    if (d)
    {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL)
        {
            if (entry->d_name[0] == '.') continue;

            char src[WLT_PKG_MAX_PATH], dst[WLT_PKG_MAX_PATH];
            snprintf(src, sizeof(src), "%s/usr/bin/%s", store_path, entry->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", bin_dir(), entry->d_name);

            /* Remove existing symlink/file, then create new */
            unlink(dst);
            if (symlink(src, dst) != 0)
            {
                /* Non-fatal — may not have write access */
                fprintf(stderr, "[wlt-pkg] WARNING: cannot symlink %s -> %s: %s\n",
                        dst, src, strerror(errno));
            }
            else
            {
                printf("[wlt-pkg] symlinked %s -> %s\n", dst, src);
            }
        }
        closedir(d);
    }

    /* Update database */
    if (!existing)
    {
        if (g_db.count >= 256) return WLT_PKG_ERR_NOMEM;
        existing = &g_db.packages[g_db.count];
        memset(existing, 0, sizeof(*existing));
        g_db.count++;
    }
    strncpy(existing->name, name, WLT_PKG_MAX_NAME - 1);
    strncpy(existing->version, "1.0.0", WLT_PKG_MAX_VERSION - 1);
    snprintf(existing->store_hash, sizeof(existing->store_hash), "%s", hash);
    existing->installed = true;
    existing->file_count = 1;

    db_save();
    printf("[wlt-pkg] installed %s v1.0.0 (store: %s)\n", name, hash);
    return WLT_PKG_OK;
}

/* ─── Remove ─── */

wlt_pkg_error_t
wlt_pkg_remove(const char *name)
{
    if (name == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    wlt_pkg_package_t *pkg = find_package(name);
    if (!pkg || !pkg->installed)
        return WLT_PKG_ERR_NOT_FOUND;

    /* Remove symlinks pointing to this store path */
    if (pkg->store_hash[0] && strcmp(pkg->store_hash, "none") != 0)
    {
        char store_path[WLT_PKG_MAX_PATH];
        snprintf(store_path, sizeof(store_path), "%s/wlt-%s-%s-1.0.0",
                 store_dir(), pkg->store_hash, name);

        /* Scan /usr/bin for symlinks pointing to this store */
        DIR *d = opendir(bin_dir());
        if (d)
        {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL)
            {
                char link_path[512];
                snprintf(link_path, sizeof(link_path), "/usr/bin/%s", entry->d_name);

                char target[512];
                ssize_t len = readlink(link_path, target, sizeof(target) - 1);
                if (len > 0)
                {
                    target[len] = '\0';
                    if (strstr(target, store_path))
                    {
                        unlink(link_path);
                        printf("[wlt-pkg] removed symlink %s\n", link_path);
                    }
                }
            }
            closedir(d);
        }

        /* Remove store directory (recursive) */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", store_path);
        int rm_rc = system(cmd); (void)rm_rc;
    }

    /* Update database */
    pkg->installed = false;
    db_save();
    printf("[wlt-pkg] removed %s\n", name);
    return WLT_PKG_OK;
}

/* ─── Update / Rollback (stubs remain, but honest) ─── */

wlt_pkg_error_t
wlt_pkg_update(const char *name)
{
    if (name == NULL) return WLT_PKG_ERR_INVALID_ARG;
    wlt_pkg_package_t *pkg = find_package(name);
    if (!pkg || !pkg->installed) return WLT_PKG_ERR_NOT_FOUND;
    /* Real update would: check repo for newer version, download, verify, swap */
    printf("[wlt-pkg] update check for %s (no repository configured)\n", name);
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_update_all(void)
{
    int checked = 0;
    for (size_t i = 0; i < g_db.count; i++)
    {
        if (g_db.packages[i].installed)
        {
            wlt_pkg_update(g_db.packages[i].name);
            checked++;
        }
    }
    printf("[wlt-pkg] checked %d packages for updates\n", checked);
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_rollback(const char *name)
{
    if (name == NULL) return WLT_PKG_ERR_INVALID_ARG;
    /* Real rollback would: find previous version in store, swap symlinks */
    printf("[wlt-pkg] rollback for %s (no previous version stored)\n", name);
    return WLT_PKG_ERR_NOT_FOUND;
}

/* ─── Query ─── */

wlt_pkg_error_t
wlt_pkg_list(wlt_pkg_database_t *db)
{
    if (db == NULL) return WLT_PKG_ERR_INVALID_ARG;
    memcpy(db, &g_db, sizeof(g_db));
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_search(const char *query, wlt_pkg_database_t *results)
{
    if (query == NULL || results == NULL) return WLT_PKG_ERR_INVALID_ARG;
    memset(results, 0, sizeof(*results));
    for (size_t i = 0; i < g_db.count && results->count < 256; i++)
    {
        if (strstr(g_db.packages[i].name, query))
        {
            results->packages[results->count++] = g_db.packages[i];
        }
    }
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_info(const char *name, wlt_pkg_package_t *pkg)
{
    if (name == NULL || pkg == NULL) return WLT_PKG_ERR_INVALID_ARG;
    wlt_pkg_package_t *found = find_package(name);
    if (!found) return WLT_PKG_ERR_NOT_FOUND;
    memcpy(pkg, found, sizeof(*pkg));
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_check_updates(char *updates, size_t max_count)
{
    if (updates == NULL) return WLT_PKG_ERR_INVALID_ARG;
    if (max_count > 0) snprintf(updates, max_count, "No repository configured");
    return WLT_PKG_OK;
}

const char *
wlt_pkg_error_str(wlt_pkg_error_t err)
{
    switch (err)
    {
        case WLT_PKG_OK:              return "OK";
        case WLT_PKG_ERR_INVALID_ARG: return "invalid argument";
        case WLT_PKG_ERR_NOT_FOUND:   return "not found";
        case WLT_PKG_ERR_IO:          return "I/O error";
        case WLT_PKG_ERR_NOMEM:       return "out of memory";
        case WLT_PKG_ERR_SIGNATURE:   return "signature verification failed";
        case WLT_PKG_ERR_EXISTS:      return "already exists";
        case WLT_PKG_ERR_CONFLICT:    return "package conflict";
        case WLT_PKG_ERR_NETWORK:     return "network error";
        case WLT_PKG_ERR_DISK_FULL:   return "disk full";
        default:                      return "unknown error";
    }
}
