/**
 * @file settings_test.c
 * @brief Unit tests for WLT-Settings state machine + error paths
 * @module wlt-settings
 * @spec: docs/specs/wlt-settings.md
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-settings/include/settings.h"

int main(void)
{
    int pass = 0, fail = 0;

    printf("=== WLT-Settings Test ===\n\n");

    /* Test 1: Create settings manager */
    {
        wlt_settings_t *s = NULL;
        wlt_settings_err_t err = wlt_settings_create(&s);
        if (err == WLT_SETTINGS_OK && s != NULL) {
            wlt_settings_destroy(s);
            printf("[PASS] Create + destroy lifecycle\n");
            pass++;
        } else {
            printf("[FAIL] Create returned %d\n", err);
            fail++;
        }
    }

    /* Test 2: Create with NULL out pointer */
    {
        wlt_settings_err_t err = wlt_settings_create(NULL);
        if (err != WLT_SETTINGS_OK) {
            printf("[PASS] Create NULL out -> error\n");
            pass++;
        } else {
            printf("[FAIL] Create NULL out returned OK\n");
            fail++;
        }
    }

    /* Test 3: Destroy NULL is safe */
    {
        wlt_settings_destroy(NULL);
        printf("[PASS] Destroy NULL does not crash\n");
        pass++;
    }

    /* Test 4: Get bool with NULL manager */
    {
        bool val = false;
        wlt_settings_err_t err = wlt_settings_get_bool(NULL, "test.key", &val);
        if (err != WLT_SETTINGS_OK) {
            printf("[PASS] Get bool NULL manager -> error\n");
            pass++;
        } else {
            printf("[FAIL] Get bool NULL manager returned OK\n");
            fail++;
        }
    }

    /* Test 5: Get int with NULL key */
    {
        wlt_settings_t *s = NULL;
        wlt_settings_create(&s);
        int64_t val = 0;
        wlt_settings_err_t err = wlt_settings_get_int(s, NULL, &val);
        if (err != WLT_SETTINGS_OK) {
            printf("[PASS] Get int NULL key -> error\n");
            pass++;
        } else {
            printf("[FAIL] Get int NULL key returned OK\n");
            fail++;
        }
        wlt_settings_destroy(s);
    }

    /* Test 6: Get string with NULL buffer */
    {
        wlt_settings_t *s = NULL;
        wlt_settings_create(&s);
        wlt_settings_err_t err = wlt_settings_get_string(s, "test.key", NULL, 0);
        if (err != WLT_SETTINGS_OK) {
            printf("[PASS] Get string NULL buf -> error\n");
            pass++;
        } else {
            printf("[FAIL] Get string NULL buf returned OK\n");
            fail++;
        }
        wlt_settings_destroy(s);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
