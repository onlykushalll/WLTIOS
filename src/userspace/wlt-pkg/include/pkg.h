/**
 * @file pkg.h
 * @brief Public API for WLT-Pkg (package manager)
 * @module wlt-pkg
 * @purpose Install, remove, update, rollback packages atomically
 * @dependencies: none
 * @tested_by: tests/userspace/pkg_test.c
 * @spec: docs/specs/wlt-pkg.md
 * @adr: docs/adr/06-musl-libc.md
 */

#ifndef WLT_PKG_H
#define WLT_PKG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_PKG_OK = 0,
    WLT_PKG_ERR_INVALID_ARG = -1,
    WLT_PKG_ERR_NOT_FOUND = -2,
    WLT_PKG_ERR_IO = -3,
    WLT_PKG_ERR_NOMEM = -4,
    WLT_PKG_ERR_SIGNATURE = -5,
    WLT_PKG_ERR_EXISTS = -6,
    WLT_PKG_ERR_CONFLICT = -7,
    WLT_PKG_ERR_NETWORK = -8,
    WLT_PKG_ERR_DISK_FULL = -9,
} wlt_pkg_error_t;

#define WLT_PKG_VERSION "1.0.0"
#define WLT_PKG_MAX_NAME 128
#define WLT_PKG_MAX_VERSION 32
#define WLT_PKG_MAX_PATH 256
#define WLT_PKG_MAX_DEPS 32
#define WLT_PKG_STORE_DIR "/var/wlt/store"
#define WLT_PKG_DB_FILE "/var/wlt/db/packages.json"
#define WLT_PKG_TOR_SOCKS "127.0.0.1:9050"

typedef struct {
    char name[WLT_PKG_MAX_NAME];
    char version[WLT_PKG_MAX_VERSION];
    char description[256];
    uint64_t size_bytes;
    uint32_t file_count;
    char dependencies[WLT_PKG_MAX_DEPS][WLT_PKG_MAX_NAME];
    size_t dep_count;
    bool installed;
    char store_hash[65];  /* SHA-256 hex */
} wlt_pkg_package_t;

typedef struct {
    wlt_pkg_package_t packages[256];
    size_t count;
} wlt_pkg_database_t;

wlt_pkg_error_t wlt_pkg_init(void);
wlt_pkg_error_t wlt_pkg_shutdown(void);

wlt_pkg_error_t wlt_pkg_install(const char *name);
wlt_pkg_error_t wlt_pkg_remove(const char *name);
wlt_pkg_error_t wlt_pkg_update(const char *name);
wlt_pkg_error_t wlt_pkg_update_all(void);
wlt_pkg_error_t wlt_pkg_rollback(const char *name);

wlt_pkg_error_t wlt_pkg_list(wlt_pkg_database_t *db);
wlt_pkg_error_t wlt_pkg_search(const char *query, wlt_pkg_database_t *results);
wlt_pkg_error_t wlt_pkg_info(const char *name, wlt_pkg_package_t *pkg);

/* Dependency resolution (dependency.c) */
wlt_pkg_error_t wlt_pkg_resolve_dependencies(const char *package_name,
                                              char resolved[][WLT_PKG_MAX_NAME],
                                              size_t *count, size_t max_count);
wlt_pkg_error_t wlt_pkg_check_conflicts(const char *package_name,
                                         char *conflict_name, size_t max_len);
bool wlt_pkg_is_installed(const char *name);

wlt_pkg_error_t wlt_pkg_check_updates(char *updates, size_t max_count);

const char *wlt_pkg_error_str(wlt_pkg_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_PKG_H */
