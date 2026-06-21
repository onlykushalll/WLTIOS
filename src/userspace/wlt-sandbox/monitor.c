/**
 * @file monitor.c
 * @brief Monitor sandboxed apps: reap exits, detect OOM and seccomp kills
 * @module wlt-sandbox/monitor
 * @purpose Watch running sandboxes, log security events, clean up on exit
 * @dependencies: sandbox.h, audit
 * @tested_by: tests/userspace/monitor_test.c
 * @spec: docs/specs/wlt-sandbox.md
 */

#include "sandbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ─── Logging ────────────────────────────────────────── */
#define WLT_MON_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/monitor][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_MON_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/monitor][WRN] " fmt "\n", ##__VA_ARGS__)
#define WLT_MON_LOG_INF(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/monitor][INF] " fmt "\n", ##__VA_ARGS__)
#define WLT_MON_LOG_DBG(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/monitor][DBG] " fmt "\n", ##__VA_ARGS__)

/* SIGSYS is delivered to a process that triggers a seccomp KILL action.
 * We use it to distinguish seccomp-killed apps from regular signal deaths. */
#ifndef SIGSYS
#define SIGSYS 31
#endif

/* Buffer for reading cgroup event files. They're tiny (a few key=value
 * pairs) but we read in a loop to handle partial reads. */
#define WLT_MON_CGROUP_BUF 512

/* ─── Cgroup Event Reading ────────────────────────────
 * memory.events looks like:
 *   oom 3
 *   oom_kill 2
 *   oom_group_kill 1
 * We parse the "oom_kill" counter (cgroup v2 OOM killer reaped a task)
 * and "oom" (cgroup hit the limit but a task may have freed memory). */

static wlt_sandbox_error_t
read_file_to_buf(const char *path, char *buf, size_t buf_max)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return WLT_SANDBOX_ERR_IO;
    size_t total = 0;
    while (total < buf_max - 1)
    {
        ssize_t n = read(fd, buf + total, buf_max - 1 - total);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            close(fd);
            return WLT_SANDBOX_ERR_IO;
        }
        if (n == 0)
            break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    close(fd);
    return WLT_SANDBOX_OK;
}

static uint64_t
parse_key_u64(const char *text, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s ", key);
    const char *p = strstr(text, pattern);
    if (p == NULL)
        return 0;
    p += strlen(pattern);
    return (uint64_t)strtoull(p, NULL, 10);
}

wlt_sandbox_error_t
wlt_sandbox_monitor_refresh_oom(wlt_SandboxApp *app)
{
    if (app == NULL || app->cgroup_path[0] == '\0')
        return WLT_SANDBOX_ERR_INVALID_ARG;

    char path[WLT_SANDBOX_MAX_PATH + 32];
    snprintf(path, sizeof(path), "%s/memory.events", app->cgroup_path);

    char buf[WLT_MON_CGROUP_BUF];
    wlt_sandbox_error_t e = read_file_to_buf(path, buf, sizeof(buf));
    if (e != WLT_SANDBOX_OK)
        return e;

    uint64_t oom_kill = parse_key_u64(buf, "oom_kill");
    uint64_t oom      = parse_key_u64(buf, "oom");

    /* The kernel counter is monotonic across the cgroup's lifetime,
     * so we record the delta since the last refresh. */
    if (oom_kill > app->oom_events)
    {
        uint32_t delta = (uint32_t)(oom_kill - app->oom_events);
        wlt_audit_log(WLT_AUDIT_KILLED, app->id, app->pid, "oom",
                      "cgroup OOM killer reaped task");
        WLT_MON_LOG_WRN("app %s: %u OOM kill events", app->id, delta);
        app->oom_events = (uint32_t)oom_kill;
        app->state = WLT_SANDBOX_OOM_KILLED;
    }
    else if (oom > app->oom_events && app->state == WLT_SANDBOX_RUNNING)
    {
        /* oom (without oom_kill) means the cgroup hit the limit but
         * reclaimed memory — log as a warning, not a kill. */
        WLT_MON_LOG_DBG("app %s: pressure event (oom=%llu)",
                        app->id, (unsigned long long)oom);
    }
    return WLT_SANDBOX_OK;
}

/* ─── Exit Handling ──────────────────────────────────── */

static void
record_exit(wlt_SandboxApp *app, int status)
{
    app->exited_at = (uint64_t)time(NULL);

    if (WIFEXITED(status))
    {
        app->exit_code = WEXITSTATUS(status);
        app->signal = 0;
        WLT_MON_LOG_INF("app %s exited (code %d)",
                        app->id, app->exit_code);
        wlt_audit_log(WLT_AUDIT_INFO, app->id, app->pid, "exit", "clean");
    }
    else if (WIFSIGNALED(status))
    {
        int sig = WTERMSIG(status);
        app->signal = sig;
        app->exit_code = 128 + sig;

        /* Seccomp KILL_PROCESS delivers SIGSYS. This is the only
         * signal we treat as a security event rather than a crash. */
        if (sig == SIGSYS)
        {
            app->state = WLT_SANDBOX_SECCOMP_KILLED;
            app->seccomp_violations++;
            WLT_MON_LOG_ERR("app %s killed by seccomp (SIGSYS)",
                            app->id);
            wlt_audit_log(WLT_AUDIT_KILLED, app->id, app->pid,
                          "seccomp", "forbidden syscall");
        }
        else
        {
            WLT_MON_LOG_WRN("app %s killed by signal %d",
                            app->id, sig);
            wlt_audit_log(WLT_AUDIT_KILLED, app->id, app->pid,
                          "signal", "crashed");
        }
    }

    /* If the OOM killer fired between the last refresh and exit,
     * the state was set to OOM_KILLED — preserve it. */
    if (app->state == WLT_SANDBOX_RUNNING)
        app->state = WLT_SANDBOX_STOPPED;
}

/* ─── Single-Step Monitor ───────────────────────────── */

/* Refresh OOM counters for every running app. Done before each
 * waitpid so an OOM event isn't missed if the app exits cleanly. */
static void
refresh_all_oom(wlt_SandboxManager *m)
{
    for (size_t i = 0; i < m->app_count; i++)
    {
        wlt_SandboxApp *app = &m->apps[i];
        if (app->state == WLT_SANDBOX_RUNNING)
            wlt_sandbox_monitor_refresh_oom(app);
    }
}

/* Sleep until SIGCHLD arrives or timeout_ms elapses. POSIX has no
 * waitpid-with-timeout, so we block SIGCHLD then sigtimedwait for it.
 * A zero timeout means "don't wait, just poll". */
static void
wait_for_sigchld(int timeout_ms)
{
    if (timeout_ms <= 0)
        return;
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    /* Sleep with SIGCHLD unblocked so we wake on child exit */
    sigdelset(&oldmask, SIGCHLD);
    struct timespec ts = { .tv_sec = timeout_ms / 1000,
                           .tv_nsec = (long)(timeout_ms % 1000) * 1000000 };
    sigtimedwait(&mask, NULL, &ts);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

wlt_sandbox_error_t
wlt_sandbox_monitor_step(wlt_SandboxManager *m, int timeout_ms)
{
    if (m == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    refresh_all_oom(m);
    wait_for_sigchld(timeout_ms);

    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid < 0)
    {
        if (errno == EINTR || errno == ECHILD)
            return WLT_SANDBOX_OK;
        WLT_MON_LOG_ERR("waitpid: %s", strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }
    if (pid == 0)
        return WLT_SANDBOX_OK;

    wlt_SandboxApp *app = wlt_sandbox_find_by_pid(m, pid);
    if (app == NULL)
    {
        /* Unknown child — probably a transient fork from the parent.
         * Reap silently to avoid zombies. */
        WLT_MON_LOG_DBG("reaped unknown pid %d", pid);
        return WLT_SANDBOX_OK;
    }

    record_exit(app, status);
    /* Tear down the cgroup now that the app is gone. The dir is
     * safe to rmdir because all processes have exited. */
    wlt_sandbox_teardown(app);
    return WLT_SANDBOX_OK;
}

/* ─── Main Monitor Loop ─────────────────────────────── */

wlt_sandbox_error_t
wlt_sandbox_monitor_loop(wlt_SandboxManager *m)
{
    if (m == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;

    WLT_MON_LOG_INF("monitor loop started (%zu apps)", m->app_count);

    /* Block SIGCHLD so sigtimedwait can receive it reliably.
     * SIGTERM/SIGINT propagate normally and break the loop. */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    while (m->app_count > 0)
    {
        wlt_sandbox_error_t e = wlt_sandbox_monitor_step(m, 1000);
        if (e != WLT_SANDBOX_OK)
            WLT_MON_LOG_WRN("monitor step: %s", wlt_sandbox_error_str(e));
    }

    WLT_MON_LOG_INF("monitor loop exiting (no apps left)");
    return WLT_SANDBOX_OK;
}
