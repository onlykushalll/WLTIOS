/**
 * @file compositor_test.c
 * @brief Unit tests for WLT-Compositor state machine + error paths
 * @module wlt-compositor
 * @spec: docs/specs/wlt-compositor.md
 *
 * Build: gcc -o compositor_test compositor_test.c \
 *   ../../src/userspace/wlt-compositor/compositor.c \
 *   ../../src/userspace/wlt-compositor/comp_output.c \
 *   ../../src/userspace/wlt-compositor/comp_input.c \
 *   ../../src/userspace/wlt-compositor/comp_view.c \
 *   -I../../src/userspace/wlt-compositor/include -DNO_WLROOTS
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-compositor/include/compositor.h"

int main(void)
{
    int pass = 0, fail = 0;

    printf("=== WLT-Compositor Test ===\n\n");

    /* Test 1: Version string exists */
    {
        const char *v = WLT_COMP_VERSION;
        if (v && v[0] != '\0') {
            printf("[PASS] Version string: %s\n", v);
            pass++;
        } else {
            printf("[FAIL] Version string is null/empty\n");
            fail++;
        }
    }

    /* Test 2: Error string for OK */
    {
        const char *s = wlt_comp_error_str(WLT_COMP_OK);
        if (s && strstr(s, "OK")) {
            printf("[PASS] Error str OK: %s\n", s);
            pass++;
        } else {
            printf("[FAIL] Error str OK returned: %s\n", s ? s : "NULL");
            fail++;
        }
    }

    /* Test 3: Error string for INVALID_ARG */
    {
        const char *s = wlt_comp_error_str(WLT_COMP_ERR_INVALID_ARG);
        if (s && strlen(s) > 0) {
            printf("[PASS] Error str INVALID_ARG: %s\n", s);
            pass++;
        } else {
            printf("[FAIL] Error str INVALID_ARG is empty\n");
            fail++;
        }
    }

    /* Test 4: Init with NULL context */
    {
        wlt_comp_error_t err = wlt_comp_init(NULL);
        if (err == WLT_COMP_ERR_INVALID_ARG) {
            printf("[PASS] Init NULL -> INVALID_ARG\n");
            pass++;
        } else {
            printf("[FAIL] Init NULL returned %d (expected %d)\n", err, WLT_COMP_ERR_INVALID_ARG);
            fail++;
        }
    }

    /* Test 5: Init + shutdown lifecycle */
    {
        wlt_comp_state_t state;
        memset(&state, 0, sizeof(state));
        wlt_comp_error_t err = wlt_comp_init(&state);
        if (err == WLT_COMP_OK) {
            wlt_comp_shutdown(&state);
            printf("[PASS] Init + shutdown lifecycle\n");
            pass++;
        } else {
            printf("[FAIL] Init returned %d\n", err);
            fail++;
        }
    }

    /* Test 6: Shutdown NULL is safe */
    {
        wlt_comp_shutdown(NULL);
        printf("[PASS] Shutdown NULL does not crash\n");
        pass++;
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
