/**
 * @file permissions_test.c
 * @brief Unit tests for WLT-Permissions error paths
 * @module wlt-permissions
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-permissions/include/permissions.h"

int main(void) {
    int pass = 0, fail = 0;
    printf("=== WLT-Permissions Test ===\n\n");

    /* Test 1: Error string for OK */
    {
        const char *s = wlt_perms_error_str(WLT_PERMS_OK);
        if (s && strlen(s) > 0) { printf("[PASS] Error str OK: %s\n", s); pass++; }
        else { printf("[FAIL] Error str OK is empty\n"); fail++; }
    }

    /* Test 2: Init with NULL manager + NULL dir */
    {
        wlt_perms_error_t err = wlt_perms_init(NULL, NULL);
        if (err != WLT_PERMS_OK) { printf("[PASS] Init NULL NULL -> error\n"); pass++; }
        else { printf("[FAIL] Init NULL NULL returned OK\n"); fail++; }
    }

    /* Test 3: Shutdown NULL is safe */
    {
        wlt_perms_shutdown(NULL);
        printf("[PASS] Shutdown NULL does not crash\n"); pass++;
    }

    /* Test 4: Error string for INVALID_ARG */
    {
        const char *s = wlt_perms_error_str(WLT_PERMS_ERR_INVALID_ARG);
        if (s && strlen(s) > 0) { printf("[PASS] Error str INVALID_ARG: %s\n", s); pass++; }
        else { printf("[FAIL] Error str INVALID_ARG is empty\n"); fail++; }
    }

    /* Test 5: Manifest error string exists */
    {
        const char *s = wlt_perms_manifest_error_str(WLT_PERMS_MFST_OK);
        if (s && strlen(s) > 0) { printf("[PASS] Manifest error str OK: %s\n", s); pass++; }
        else { printf("[FAIL] Manifest error str OK is empty\n"); fail++; }
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
