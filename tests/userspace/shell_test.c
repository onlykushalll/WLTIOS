/**
 * @file shell_test.c
 * @brief Unit tests for WLT-Shell state machine + error paths
 * @module wlt-shell
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-shell/include/shell.h"

int main(void) {
    int pass = 0, fail = 0;
    printf("=== WLT-Shell Test ===\n\n");

    /* Test 1: Error string for OK */
    {
        const char *s = wlt_shell_error_str(WLT_SHELL_OK);
        if (s && strlen(s) > 0) { printf("[PASS] Error str OK: %s\n", s); pass++; }
        else { printf("[FAIL] Error str OK is empty\n"); fail++; }
    }

    /* Test 2: Error string for INVALID_ARG */
    {
        const char *s = wlt_shell_error_str(WLT_SHELL_ERR_INVALID_ARG);
        if (s && strlen(s) > 0) { printf("[PASS] Error str INVALID_ARG: %s\n", s); pass++; }
        else { printf("[FAIL] Error str INVALID_ARG is empty\n"); fail++; }
    }

    /* Test 3: Init with NULL */
    {
        wlt_shell_error_t err = wlt_shell_init(NULL);
        if (err == WLT_SHELL_ERR_INVALID_ARG) { printf("[PASS] Init NULL -> INVALID_ARG\n"); pass++; }
        else { printf("[FAIL] Init NULL returned %d\n", err); fail++; }
    }

    /* Test 4: Init + shutdown lifecycle */
    {
        wlt_shell_state_t state;
        memset(&state, 0, sizeof(state));
        wlt_shell_error_t err = wlt_shell_init(&state);
        if (err == WLT_SHELL_OK) {
            wlt_shell_shutdown(&state);
            printf("[PASS] Init + shutdown lifecycle\n"); pass++;
        } else { printf("[FAIL] Init returned %d\n", err); fail++; }
    }

    /* Test 5: Shutdown NULL is safe */
    {
        wlt_shell_shutdown(NULL);
        printf("[PASS] Shutdown NULL does not crash\n"); pass++;
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
