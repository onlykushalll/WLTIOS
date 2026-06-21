/**
 * @file monitor.c
 * @brief Process monitoring implementation
 * @module wlt-init/monitor
 * @purpose Wait for child processes, handle crashes
 * @spec: docs/specs/wlt-init.md
 */

#include "monitor.h"
#include "service.h"
#include "log.h"
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

wlt_init_error_t
wlt_monitor_loop(wlt_init_t *init)
{
    /* TODO(wlt-init): Implement monitor loop */
    /* Loop: */
    /*   1. waitpid(-1, &status, 0) — wait for any child */
    /*   2. Find which service died (match pid) */
    /*   3. wlt_monitor_handle_crash(init, svc, exit_code) */
    /*   4. Check Tor status (handle kill switch if needed) */
    /*   5. Repeat forever */
    (void)init;
    while (1)
    {
        sleep(1);  /* TODO: replace with waitpid */
    }
    return WLT_INIT_OK;
}

void
wlt_monitor_handle_crash(wlt_init_t *init, wlt_service_t *svc, int exit_code)
{
    WLT_LOG_WRN("Service %s crashed (exit %d)", svc->name, exit_code);

    if (wlt_monitor_should_restart(svc, exit_code))
    {
        if (svc->priority == WLT_PRIO_CRITICAL)
        {
            /* Critical: restart immediately */
            WLT_LOG_INF("Restarting critical service %s immediately", svc->name);
            wlt_service_restart(init, svc);
        }
        else
        {
            /* Normal: restart with backoff */
            uint32_t backoff = svc->restart_backoff_ms * (svc->restart_count + 1);
            WLT_LOG_INF("Restarting %s in %u ms (attempt %u)",
                         svc->name, backoff, svc->restart_count + 1);
            /* TODO(wlt-init): Schedule restart after backoff */
        }
        svc->restart_count++;
    }
    else
    {
        svc->state = WLT_SVC_FAILED;
        WLT_LOG_ERR("Service %s marked as failed (no restart)", svc->name);
    }
}

bool
wlt_monitor_should_restart(const wlt_service_t *svc, int exit_code)
{
    if (!svc->restart_on_crash)
        return false;
    /* Don't restart if exit was clean (code 0) */
    if (exit_code == 0)
        return false;
    /* Don't restart low-priority services */
    if (svc->priority == WLT_PRIO_LOW)
        return false;
    return true;
}
