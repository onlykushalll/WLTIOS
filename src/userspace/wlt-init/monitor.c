/**
 * @file monitor.c
 * @brief Process monitoring implementation
 * @module wlt-init/monitor
 * @purpose Monitors running services, restarts crashed ones
 * @dependencies: monitor.h, service.h, init.h, log.h, tor.h
 * @tested_by: tests/userspace/monitor_test.c
 * @spec: docs/specs/wlt-init.md
 */

#include "monitor.h"
#include "service.h"
#include "tor.h"
#include "log.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>

/* ─── Internal: Find service by PID ──────────────────── */

static wlt_service_t *
find_by_pid(wlt_init_t *init, pid_t pid)
{
    for (size_t i = 0; i < init->service_count; i++)
    {
        if (init->services[i].pid == pid)
        {
            return &init->services[i];
        }
    }
    return NULL;
}

/* ─── Internal: Restart with backoff ────────────────── */

static void
schedule_restart(wlt_init_t *init, wlt_service_t *svc)
{
    if (!wlt_monitor_should_restart(svc, 1))
    {
        svc->state = WLT_SVC_FAILED;
        WLT_LOG_ERR("Service %s marked as failed (no restart)", svc->name);
        return;
    }

    /* Calculate backoff: base * (restart_count + 1), max 60s */
    uint32_t backoff = svc->restart_backoff_ms * (svc->restart_count + 1);
    if (backoff > 60000)
        backoff = 60000;

    if (svc->priority == WLT_PRIO_CRITICAL)
    {
        /* Critical services restart immediately */
        WLT_LOG_INF("Restarting critical service %s immediately", svc->name);
        wlt_service_restart(init, svc);
    }
    else
    {
        WLT_LOG_INF("Restarting %s in %u ms (attempt %u)",
                     svc->name, backoff, svc->restart_count + 1);

        /* Simple sleep-based restart (TODO: use event loop) */
        usleep(backoff * 1000);
        wlt_service_restart(init, svc);
    }

    svc->restart_count++;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_init_error_t
wlt_monitor_loop(wlt_init_t *init)
{
    if (init == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    WLT_LOG_INF("Monitor loop started");

    while (1)
    {
        int status;
        pid_t pid;

        /* Wait for any child to exit */
        pid = waitpid(-1, &status, 0);

        if (pid < 0)
        {
            if (errno == EINTR)
            {
                /* Signal interrupted — check for shutdown */
                /* TODO(wlt-init): Handle SIGTERM for clean shutdown */
                continue;
            }
            WLT_LOG_ERR("waitpid failed: %s", strerror(errno));
            sleep(1);
            continue;
        }

        /* Find which service died */
        wlt_service_t *svc = find_by_pid(init, pid);
        if (svc == NULL)
        {
            /* Unknown child process — ignore */
            WLT_LOG_DBG("Unknown child exited: PID %d", pid);
            continue;
        }

        /* Determine exit info */
        int exit_code = -1;
        if (WIFEXITED(status))
        {
            exit_code = WEXITSTATUS(status);
            WLT_LOG_INF("Service %s exited (code %d)", svc->name, exit_code);
        }
        else if (WIFSIGNALED(status))
        {
            int sig = WTERMSIG(status);
            WLT_LOG_WRN("Service %s killed by signal %d", svc->name, sig);
            exit_code = 128 + sig;
        }

        /* Update service state */
        svc->pid = -1;
        svc->state = WLT_SVC_STOPPED;

        /* Handle crash (restart if configured) */
        wlt_monitor_handle_crash(init, svc, exit_code);
    }

    /* Should never reach here */
    return WLT_INIT_OK;
}

void
wlt_monitor_handle_crash(wlt_init_t *init, wlt_service_t *svc, int exit_code)
{
    if (init == NULL || svc == NULL)
        return;

    /* Clean exit (code 0) — don't restart */
    if (exit_code == 0)
    {
        WLT_LOG_INF("Service %s exited cleanly", svc->name);
        svc->state = WLT_SVC_STOPPED;
        return;
    }

    WLT_LOG_WRN("Service %s crashed (exit %d)", svc->name, exit_code);

    if (!wlt_monitor_should_restart(svc, exit_code))
    {
        svc->state = WLT_SVC_FAILED;
        WLT_LOG_ERR("Service %s marked as failed", svc->name);
        return;
    }

    /* Don't restart if Tor is not ready and service requires Tor */
    if (svc->tor_required && !wlt_tor_is_ready(init))
    {
        WLT_LOG_WRN("Service %s requires Tor — waiting for Tor before restart",
                     svc->name);
        svc->state = WLT_SVC_FROZEN;
        return;
    }

    /* Don't restart if kill switch is active */
    if (init->kill_switch_active)
    {
        WLT_LOG_WRN("Kill switch active — not restarting %s", svc->name);
        svc->state = WLT_SVC_FROZEN;
        return;
    }

    schedule_restart(init, svc);
}

bool
wlt_monitor_should_restart(const wlt_service_t *svc, int exit_code)
{
    if (svc == NULL)
        return false;

    /* Don't restart if not configured */
    if (!svc->restart_on_crash)
        return false;

    /* Don't restart if exit was clean */
    if (exit_code == 0)
        return false;

    /* Don't restart low-priority services */
    if (svc->priority == WLT_PRIO_LOW)
        return false;

    /* Don't restart if too many attempts (max 10) */
    if (svc->restart_count >= 10)
    {
        WLT_LOG_ERR("Service %s exceeded max restarts (10)", svc->name);
        return false;
    }

    return true;
}
