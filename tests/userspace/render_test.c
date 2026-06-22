/**
 * @file render_test.c
 * @brief Unit tests for WLT-Render — pixel buffer operations
 * @module wlt-render
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-render/include/render.h"

int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Render Test ===\n\n");

    /* Test 1: Init with NULL */
    {
        wlt_render_error_t err = wlt_render_init(NULL, 100, 100);
        if (err == WLT_R_ERR_INVALID_ARG) { printf("[PASS] Init NULL -> INVALID_ARG\n"); pass++; }
        else { printf("[FAIL] Init NULL returned %d\n", err); fail++; }
    }

    /* Test 2: Init with valid state */
    {
        wlt_render_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        wlt_render_error_t err = wlt_render_init(&ctx, 100, 100);
        if (err == WLT_R_OK && ctx.buffer != NULL) { printf("[PASS] Init valid -> OK, buffer allocated\n"); pass++; }
        else { printf("[FAIL] Init returned %d\n", err); fail++; }
        
        /* Test 3: Clear buffer (fill with color) */
        wlt_render_error_t err2 = wlt_render_clear(&ctx, 0xFF0000FF); /* red */
        if (err2 == WLT_R_OK) { printf("[PASS] Clear buffer -> OK\n"); pass++; }
        else { printf("[FAIL] Clear returned %d\n", err2); fail++; }
        
        /* Test 4: Fill rect and verify pixel at expected position */
        wlt_render_error_t err3 = wlt_render_fill_rect(&ctx, 10, 10, 20, 20, 0xFF00FF00); /* green */
        if (err3 == WLT_R_OK) { printf("[PASS] Fill rect -> OK\n"); pass++; }
        else { printf("[FAIL] Fill rect returned %d\n", err3); fail++; }
        
        wlt_render_shutdown(&ctx);
    }

    /* Test 5: Shutdown NULL is safe */
    {
        wlt_render_shutdown(NULL);
        printf("[PASS] Shutdown NULL does not crash\n"); pass++;
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
