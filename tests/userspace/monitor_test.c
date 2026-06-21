/**
 * @file monitor_test.c
 * @brief Unit tests for WLT-Init process monitor
 * @module wlt-init/monitor
 * @purpose Test crash detection + restart logic
 * @spec: docs/specs/wlt-init.md
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../src/userspace/wlt-init/include/init.h"
#include "../../src/userspace/wlt-init/include/monitor.h"

/* Stubs */
const char *wlt_init_error_str(wlt_init_error_t err) {
    switch (err) {
        case WLT_INIT_OK: return "OK";
        default: return "Error";
    }
}

int main(void)
{
    int pass = 0, fail = 0;
    wlt_init_t init;
    wlt_service_t svc;

    memset(&init, 0, sizeof(init));
    init.tor_ready = true;
    init.kill_switch_active = false;

    printf("=== WLT-Init Monitor Test ===\n\n");

    /* Test 1: should_restart = false for clean exit (code 0) */
    memset(&svc, 0, sizeof(svc));
    svc.restart_on_crash = true;
    svc.priority = WLT_PRIO_NORMAL;
    svc.restart_count = 0;

    if (!wlt_monitor_should_restart(&svc, 0))
    {
        printf("[PASS] Test 1: No restart on clean exit (code 0)\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 1: Should not restart on clean exit\n");
        fail++;
    }

    /* Test 2: should_restart = true for crash (code 1) */
    if (wlt_monitor_should_restart(&svc, 1))
    {
        printf("[PASS] Test 2: Restart on crash (code 1)\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 2: Should restart on crash\n");
        fail++;
    }

    /* Test 3: should_restart = false if restart_on_crash = false */
    svc.restart_on_crash = false;
    if (!wlt_monitor_should_restart(&svc, 1))
    {
        printf("[PASS] Test 3: No restart when restart_on_crash=false\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 3: Should not restart when disabled\n");
        fail++;
    }

    /* Test 4: should_restart = false for low priority */
    svc.restart_on_crash = true;
    svc.priority = WLT_PRIO_LOW;
    if (!wlt_monitor_should_restart(&svc, 1))
    {
        printf("[PASS] Test 4: No restart for low priority\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 4: Should not restart low priority\n");
        fail++;
    }

    /* Test 5: should_restart = false after 10 attempts */
    svc.priority = WLT_PRIO_NORMAL;
    svc.restart_count = 10;
    if (!wlt_monitor_should_restart(&svc, 1))
    {
        printf("[PASS] Test 5: No restart after 10 attempts\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 5: Should stop after 10 restarts\n");
        fail++;
    }

    /* Test 6: should_restart = true for signal kill (code 128+9) */
    svc.restart_count = 0;
    if (wlt_monitor_should_restart(&svc, 137))  /* SIGKILL */
    {
        printf("[PASS] Test 6: Restart after SIGKILL (code 137)\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 6: Should restart after signal kill\n");
        fail++;
    }

    /* Test 7: handle_crash marks as STOPPED on clean exit */
    memset(&svc, 0, sizeof(svc));
    svc.restart_on_crash = true;
    svc.priority = WLT_PRIO_NORMAL;
    svc.state = WLT_SVC_RUNNING;
    wlt_monitor_handle_crash(&init, &svc, 0);
    if (svc.state == WLT_SVC_STOPPED)
    {
        printf("[PASS] Test 7: Clean exit → STOPPED\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 7: Expected STOPPED, got %s\n",
               wlt_service_state_str(svc.state));
        fail++;
    }

    /* Test 8: handle_crash marks as FAILED when no restart */
    svc.restart_on_crash = false;
    svc.state = WLT_SVC_RUNNING;
    wlt_monitor_handle_crash(&init, &svc, 1);
    if (svc.state == WLT_SVC_FAILED)
    {
        printf("[PASS] Test 8: Crash + no restart → FAILED\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 8: Expected FAILED, got %s\n",
               wlt_service_state_str(svc.state));
        fail++;
    }

    /* Test 9: handle_crash FROZEN when kill switch active */
    svc.restart_on_crash = true;
    svc.priority = WLT_PRIO_NORMAL;
    svc.state = WLT_SVC_RUNNING;
    svc.restart_count = 0;
    init.kill_switch_active = true;
    wlt_monitor_handle_crash(&init, &svc, 1);
    if (svc.state == WLT_SVC_FROZEN)
    {
        printf("[PASS] Test 9: Kill switch active → FROZEN\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 9: Expected FROZEN, got %s\n",
               wlt_service_state_str(svc.state));
        fail++;
    }

    printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
