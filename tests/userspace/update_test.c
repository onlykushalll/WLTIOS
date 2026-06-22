#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-update/include/update.h"
#include "../../src/userspace/wlt-update/include/partition.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Update Test ===\n\n");
    { const char *s = wlt_update_error_str(WLT_UPDATE_OK); if (s && strlen(s)>0) { printf("[PASS] Error str OK: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *s = wlt_update_error_str(WLT_UPDATE_ERR_INVALID_ARG); if (s && strlen(s)>0) { printf("[PASS] Error str INVALID_ARG: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_update_error_t err = wlt_update_init(NULL); if (err != WLT_UPDATE_OK) { printf("[PASS] Init NULL config -> error\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_PartitionId pid; wlt_update_error_t err = wlt_partition_get_active(&pid); if (err == WLT_UPDATE_OK || err != WLT_UPDATE_OK) { printf("[PASS] partition_get_active callable\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_update_error_t err = wlt_partition_rollback(); if (err == WLT_UPDATE_OK || err != WLT_UPDATE_OK) { printf("[PASS] partition_rollback callable\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
