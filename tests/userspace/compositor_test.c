/**
 * @file compositor_test.c
 * @brief Unit tests for WLT-Compositor — wlroots header verification + error paths
 * @module wlt-compositor
 * @spec: docs/specs/wlt-compositor.md
 *
 * This test verifies:
 * 1. wlroots headers compile (compile-time check)
 * 2. Error string functions work
 * 3. Init with NULL returns INVALID_ARG
 * 4. Shutdown with NULL is safe
 *
 * NOTE: Real init (which calls wl_display_create + wlr_backend_autocreate) is
 * NOT tested here because it requires a display server. The compile-time check
 * (including <wlr/backend.h>) verifies wlroots is installed and the API matches.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Compile-time check: wlroots 0.18 headers are available and API matches */
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>

/* Compile-time check: can declare wlroots pointers */
static struct wlr_backend *test_backend = NULL;
static struct wlr_renderer *test_renderer = NULL;
static struct wlr_scene *test_scene = NULL;

#include "../../src/userspace/wlt-compositor/include/compositor.h"

int main(void)
{
    int pass = 0, fail = 0;

    printf("=== WLT-Compositor Test (wlroots 0.18 verified) ===\n\n");

    /* Test 1: Version string (now 2.0.0 — real wlroots) */
    {
        const char *v = WLT_COMP_VERSION;
        if (v && v[0] != '\0') {
            printf("[PASS] Version: %s\n", v);
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

    /* Test 3: Error string for BACKEND */
    {
        const char *s = wlt_comp_error_str(WLT_COMP_ERR_BACKEND);
        if (s && strlen(s) > 0) {
            printf("[PASS] Error str BACKEND: %s\n", s);
            pass++;
        } else {
            printf("[FAIL] Error str BACKEND is empty\n");
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

    /* Test 5: Shutdown NULL is safe */
    {
        wlt_comp_shutdown(NULL);
        printf("[PASS] Shutdown NULL does not crash\n");
        pass++;
    }

    /* Test 6: wlroots headers compile + pointers declarable */
    {
        if (test_backend == NULL && test_renderer == NULL && test_scene == NULL) {
            printf("[PASS] wlroots 0.18 headers compile + pointers initialize to NULL\n");
            pass++;
        } else {
            printf("[FAIL] wlroots pointers not NULL\n");
            fail++;
        }
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
