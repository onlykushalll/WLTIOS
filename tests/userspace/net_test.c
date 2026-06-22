#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-net/include/net.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Net Test ===\n\n");
    { const char *s = wlt_net_error_str(WLT_NET_OK); if (s && strlen(s)>0) { printf("[PASS] Error str OK: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *s = wlt_net_error_str(WLT_NET_ERR_INVALID_ARG); if (s && strlen(s)>0) { printf("[PASS] Error str INVALID_ARG: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_net_error_t err = wlt_net_config_defaults(NULL); if (err != WLT_NET_OK) { printf("[PASS] Config defaults NULL -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_net_error_t err = wlt_net_shutdown(NULL); if (err != WLT_NET_OK) { printf("[PASS] Shutdown NULL -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_net_error_t err = wlt_net_kill_switch_trigger(NULL); if (err != WLT_NET_OK) { printf("[PASS] Kill switch NULL -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
