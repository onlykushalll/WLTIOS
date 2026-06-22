#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-sandbox/include/sandbox.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Sandbox Test ===\n\n");
    { const char *s = wlt_sandbox_error_str(WLT_SANDBOX_OK); if (s && strlen(s)>0) { printf("[PASS] Error str OK: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *s = wlt_sandbox_error_str(WLT_SANDBOX_ERR_INVALID_ARG); if (s && strlen(s)>0) { printf("[PASS] Error str INVALID_ARG: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_sandbox_shutdown(NULL); printf("[PASS] Shutdown NULL does not crash\n"); pass++; }
    { const char *s = wlt_sandbox_error_str(WLT_SANDBOX_ERR_PARSE); if (s && strlen(s)>0) { printf("[PASS] Error str PARSE: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *s = wlt_sandbox_error_str(WLT_SANDBOX_ERR_CGROUP); if (s && strlen(s)>0) { printf("[PASS] Error str CGROUP: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
