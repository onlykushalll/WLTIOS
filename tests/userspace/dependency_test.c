/**
 * @file dependency_test.c
 * @brief Unit tests for WLT-Init dependency resolver (topological sort)
 * @module wlt-init/dependency
 * @purpose Test service startup order resolution + cycle detection
 * @spec: docs/specs/wlt-init.md
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../src/userspace/wlt-init/include/init.h"
#include "../../src/userspace/wlt-init/include/service.h"
#include "../../src/userspace/wlt-init/include/dependency.h"

/* Stubs */
const char *wlt_init_error_str(wlt_init_error_t err) {
    switch (err) {
        case WLT_INIT_OK: return "OK";
        default: return "Error";
    }
}

static wlt_init_t g_init;

static void
setup_services(void)
{
    memset(&g_init, 0, sizeof(g_init));
    g_init.tor_ready = true;
    g_init.kill_switch_active = false;

    /* Service A: no deps */
    strcpy(g_init.services[0].name, "A");
    g_init.services[0].depend_count = 0;
    g_init.services[0].state = WLT_SVC_RUNNING;
    g_init.services[0].pid = -1;
    g_init.services[0].restart_count = 0;

    /* Service B: depends on A */
    strcpy(g_init.services[1].name, "B");
    strcpy(g_init.services[1].depends[0], "A");
    g_init.services[1].depend_count = 1;
    g_init.services[1].state = WLT_SVC_STOPPED;

    /* Service C: depends on B */
    strcpy(g_init.services[2].name, "C");
    strcpy(g_init.services[2].depends[0], "B");
    g_init.services[2].depend_count = 1;
    g_init.services[2].state = WLT_SVC_STOPPED;

    g_init.service_count = 3;
}

int main(void)
{
    int pass = 0, fail = 0;

    printf("=== WLT-Init Dependency Resolver Test ===\n\n");

    setup_services();

    /* Test 1: Resolve simple chain A → B → C */
    size_t order[WLT_INIT_MAX_SERVICES];
    size_t count = 0;

    wlt_init_error_t err = wlt_dependency_resolve(&g_init, order, &count);
    if (err == WLT_INIT_OK && count == 3)
    {
        /* Expected: A (index 0) first, then B (1), then C (2) */
        if (order[0] == 0 && order[1] == 1 && order[2] == 2)
        {
            printf("[PASS] Test 1: Chain A → B → C resolved in order\n");
            pass++;
        }
        else
        {
            printf("[FAIL] Test 1: Expected order 0,1,2 got %zu,%zu,%zu\n",
                   order[0], order[1], order[2]);
            fail++;
        }
    }
    else
    {
        printf("[FAIL] Test 1: Resolve failed (err=%d, count=%zu)\n", err, count);
        fail++;
    }

    /* Test 2: Check deps_running — B's dep (A) is running */
    printf("Debug: about to check deps_running for B (service_count=%zu)\n", g_init.service_count);
    if (wlt_dependency_deps_running(&g_init, &g_init.services[1]))
    {
        printf("[PASS] Test 2: B's deps (A) are running\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 2: B's deps should be running\n");
        fail++;
    }

    /* Test 3: Check deps_running — C's dep (B) is NOT running */
    if (!wlt_dependency_deps_running(&g_init, &g_init.services[2]))
    {
        printf("[PASS] Test 3: C's deps (B) are NOT running\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 3: C's deps should NOT be running\n");
        fail++;
    }

    /* Test 4: No cycle in current graph */
    if (!wlt_dependency_has_cycle(&g_init))
    {
        printf("[PASS] Test 4: No cycle detected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 4: False cycle detected\n");
        fail++;
    }

    /* Test 5: Create a cycle (A depends on C, C depends on B, B depends on A) */
    strcpy(g_init.services[0].depends[0], "C");
    g_init.services[0].depend_count = 1;
    g_init.services[0].state = WLT_SVC_STOPPED;

    if (wlt_dependency_has_cycle(&g_init))
    {
        printf("[PASS] Test 5: Cycle A→C→B→A detected\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 5: Cycle not detected\n");
        fail++;
    }

    printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
