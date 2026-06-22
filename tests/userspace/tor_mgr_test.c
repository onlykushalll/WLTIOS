#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-tor-mgr/include/tor_mgr.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Tor-Mgr Test ===\n\n");
    { const char *v = WLT_TOR_MGR_VERSION; if (v && v[0]) { printf("[PASS] Version: %s\n", v); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *n = WLT_TOR_MGR_NAME; if (n && n[0]) { printf("[PASS] Name: %s\n", n); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { if (WLT_TOR_MAX_CIRCUITS > 0) { printf("[PASS] Max circuits: %d\n", WLT_TOR_MAX_CIRCUITS); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { if (WLT_TOR_MAX_NODES > 0) { printf("[PASS] Max nodes: %d\n", WLT_TOR_MAX_NODES); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_tor_mgr_shutdown(NULL); printf("[PASS] Shutdown NULL does not crash\n"); pass++; }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
