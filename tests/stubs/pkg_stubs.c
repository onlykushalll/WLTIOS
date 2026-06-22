/* Stubs for WLT-Pkg — test the API surface without external deps */
#include "../../src/userspace/wlt-pkg/include/pkg.h"
const char *wlt_pkg_error_str(wlt_pkg_error_t e) {
    switch(e) { case WLT_PKG_OK: return "OK"; case WLT_PKG_ERR_INVALID_ARG: return "Invalid argument";
    case WLT_PKG_ERR_NOT_FOUND: return "Not found"; default: return "Error"; } }
wlt_pkg_error_t wlt_pkg_init(void) { return WLT_PKG_OK; }
wlt_pkg_error_t wlt_pkg_shutdown(void) { return WLT_PKG_OK; }
wlt_pkg_error_t wlt_pkg_install(const char *n) { return n ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_remove(const char *n) { return n ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_update(const char *n) { return n ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_update_all(void) { return WLT_PKG_OK; }
wlt_pkg_error_t wlt_pkg_rollback(const char *n) { return n ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_list(wlt_pkg_database_t *db) { return db ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_search(const char *q, wlt_pkg_database_t *r) { return (q&&r) ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_info(const char *n, wlt_pkg_package_t *p) { return (n&&p) ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
wlt_pkg_error_t wlt_pkg_check_updates(char *u, size_t m) { return u ? WLT_PKG_OK : WLT_PKG_ERR_INVALID_ARG; }
