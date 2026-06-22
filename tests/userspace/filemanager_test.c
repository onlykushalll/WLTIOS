#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-filemanager/include/filemanager.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-FileManager Test ===\n\n");
    { const char *s = wlt_fm_error_str(WLT_FM_OK); if (s && strlen(s)>0) { printf("[PASS] Error str OK: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_filemanager_error_t err = wlt_fm_init(NULL, "/tmp"); if (err != WLT_FM_OK) { printf("[PASS] Init NULL state -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_fm_state_t state; memset(&state, 0, sizeof(state)); wlt_filemanager_error_t err = wlt_fm_init(&state, NULL); if (err != WLT_FM_OK) { printf("[PASS] Init NULL path -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_filemanager_error_t err = wlt_fm_navigate(NULL, "/tmp"); if (err != WLT_FM_OK) { printf("[PASS] Navigate NULL state -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_filemanager_error_t err = wlt_fm_navigate_up(NULL); if (err != WLT_FM_OK) { printf("[PASS] Navigate_up NULL -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
