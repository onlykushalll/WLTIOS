/**
 * @file widgets_test.c
 * @brief Unit tests for WLT-Widgets — widget tree operations
 * @module wlt-widgets
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-widgets/include/widgets.h"

int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Widgets Test ===\n\n");

    /* Test 1: Create a button widget */
    {
        wlt_widget_t *w = wlt_widget_create(WLT_W_TYPE_BUTTON);
        if (w != NULL && w->type == WLT_W_TYPE_BUTTON) { printf("[PASS] Create button -> OK\n"); pass++; }
        else { printf("[FAIL] Create button returned NULL or wrong type\n"); fail++; }
        wlt_widget_destroy(w);
    }

    /* Test 2: Add child to parent (parent owns child — don't destroy child separately) */
    {
        wlt_widget_t *parent = wlt_widget_create(WLT_W_TYPE_CONTAINER);
        wlt_widget_t *child = wlt_widget_create(WLT_W_TYPE_BUTTON);
        wlt_widget_error_t err = wlt_widget_add_child(parent, child);
        if (err == WLT_W_OK && parent->child_count == 1) { printf("[PASS] Add child -> OK, count=1\n"); pass++; }
        else { printf("[FAIL] Add child returned %d, count=%zu\n", err, parent->child_count); fail++; }
        /* Only destroy parent — it recursively destroys children */
        wlt_widget_destroy(parent);
    }

    /* Test 3: Add child with NULL parent */
    {
        wlt_widget_t *child = wlt_widget_create(WLT_W_TYPE_BUTTON);
        wlt_widget_error_t err = wlt_widget_add_child(NULL, child);
        if (err != WLT_W_OK) { printf("[PASS] Add child NULL parent -> error\n"); pass++; }
        else { printf("[FAIL] Add child NULL parent returned OK\n"); fail++; }
        wlt_widget_destroy(child);
    }

    /* Test 4: Create and destroy lifecycle */
    {
        wlt_widget_t *w = wlt_widget_create(WLT_W_TYPE_LABEL);
        if (w != NULL) {
            wlt_widget_destroy(w);
            printf("[PASS] Create + destroy lifecycle\n"); pass++;
        } else {
            printf("[FAIL] Create label returned NULL\n"); fail++;
        }
    }

    /* Test 5: Destroy NULL is safe */
    {
        wlt_widget_destroy(NULL);
        printf("[PASS] Destroy NULL does not crash\n"); pass++;
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
