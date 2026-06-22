#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-pkg/include/pkg.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Pkg Test ===\n\n");
    { const char *v = WLT_PKG_VERSION; if (v && v[0]) { printf("[PASS] Version: %s\n", v); pass++; } else { printf("[FAIL] Version empty\n"); fail++; } }
    { const char *s = wlt_pkg_error_str(WLT_PKG_OK); if (s && strlen(s)>0) { printf("[PASS] Error str OK: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *s = wlt_pkg_error_str(WLT_PKG_ERR_NOT_FOUND); if (s && strlen(s)>0) { printf("[PASS] Error str NOT_FOUND: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_pkg_error_t err = wlt_pkg_init(); if (err == WLT_PKG_OK) { printf("[PASS] Init OK\n"); pass++; } else { printf("[FAIL] Init returned %d\n", err); fail++; } }
    { wlt_pkg_shutdown(); printf("[PASS] Shutdown (no crash)\n"); pass++; }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
