/**
 * @file audit.c
 * @brief Append-only security audit log for WLT-Sandbox
 * @module wlt-sandbox/audit
 * @purpose Records sandbox lifecycle and security events for forensics
 * @dependencies: sandbox.h
 * @tested_by: tests/userspace/audit_test.c
 * @spec: docs/specs/wlt-sandbox.md
 */

#include "sandbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ─── Logging ──────────────────────────────────────────
 * The audit module is the lowest layer — it must not call the
 * sandbox logger (which would re-enter through stderr). Errors
 * here go to stderr directly. */
#define WLT_AUDIT_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/audit][ERR] " fmt "\n", ##__VA_ARGS__)

/* Single global fd, opened once at init. We use a process-wide singleton
 * because the launcher and monitor share an address space (the daemon)
 * and concurrent writes to the same fd with O_APPEND are atomic on
 * Linux for writes ≤ PIPE_BUF (4096 bytes). */
static int g_audit_fd = -1;

/* Bounded string copy — same pattern as the rest of the codebase. */
static void
copy_bounded(char *dst, size_t dst_max, const char *src)
{
    if (dst_max == 0)
        return;
    size_t n = strlen(src);
    if (n >= dst_max)
        n = dst_max - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Format the current UTC time as ISO 8601 (e.g. 2026-06-22T13:01:07Z).
 * Returns 0 on success, -1 if the buffer is too small. */
static int
format_iso8601_utc(uint64_t epoch_seconds, char *buf, size_t buf_max)
{
    time_t t = (time_t)epoch_seconds;
    struct tm tm;
    if (gmtime_r(&t, &tm) == NULL)
        return -1;
    if (strftime(buf, buf_max, "%Y-%m-%dT%H:%M:%SZ", &tm) == 0)
        return -1;
    return 0;
}

static const char *
level_str(wlt_audit_level_t level)
{
    switch (level)
    {
        case WLT_AUDIT_INFO:   return "INFO";
        case WLT_AUDIT_WARN:   return "WARN";
        case WLT_AUDIT_DENIED: return "DENIED";
        case WLT_AUDIT_KILLED: return "KILLED";
        default:               return "UNKNOWN";
    }
}

/* Sanitise a string for the log: replace newlines/tabs with spaces
 * and truncate to a safe length. Audit logs are grep-friendly, so
 * we must not let an attacker inject fake log lines via their app id. */
static void
sanitise(char *buf, size_t buf_max)
{
    for (size_t i = 0; i < buf_max && buf[i] != '\0'; i++)
    {
        char c = buf[i];
        if (c == '\n' || c == '\r' || c == '\t')
            buf[i] = ' ';
    }
}

/* ─── Public API ─────────────────────────────────────── */

wlt_sandbox_error_t
wlt_audit_init(void)
{
    if (g_audit_fd >= 0)
        return WLT_SANDBOX_OK;  /* already open */

    /* Ensure the parent directory exists. Mode 0750 keeps the audit
     * log readable only by root and the wlt group. */
    if (mkdir(WLT_SANDBOX_LOG_DIR, 0750) != 0 && errno != EEXIST)
    {
        WLT_AUDIT_LOG_ERR("mkdir(%s): %s",
                          WLT_SANDBOX_LOG_DIR, strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }

    /* O_APPEND: every write atomically extends the file, so concurrent
     * writers don't clobber each other (kernel holds i_mutex).
     * O_CLOEXEC: the launcher's forked child must not inherit this fd
     * or it would race with the parent's writes. */
    g_audit_fd = open(WLT_SANDBOX_AUDIT_LOG,
                      O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                      0640);
    if (g_audit_fd < 0)
    {
        WLT_AUDIT_LOG_ERR("open(%s): %s",
                          WLT_SANDBOX_AUDIT_LOG, strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }

    /* Write a separator line so log readers can spot restarts. */
    const char *boot = "# wlt-sandbox audit log opened\n";
    ssize_t n = write(g_audit_fd, boot, strlen(boot));
    (void)n;
    return WLT_SANDBOX_OK;
}

void
wlt_audit_close(void)
{
    if (g_audit_fd >= 0)
    {
        close(g_audit_fd);
        g_audit_fd = -1;
    }
}

void
wlt_audit_log(wlt_audit_level_t level, const char *app_id, pid_t pid,
              const char *event, const char *detail)
{
    if (g_audit_fd < 0)
    {
        /* Lazy-init so callers don't have to check return values.
         * If init fails, we silently drop the event — better than
         * crashing the sandbox over a log write. */
        if (wlt_audit_init() != WLT_SANDBOX_OK)
            return;
    }

    /* Build the line in a local buffer first so we issue a single
     * write() and stay within PIPE_BUF for atomic appends. */
    char line[512];
    char ts[32];
    if (format_iso8601_utc((uint64_t)time(NULL), ts, sizeof(ts)) != 0)
        copy_bounded(ts, sizeof(ts), "?");

    /* Sanitise user-controlled fields. The level and event strings
     * are constants from our own code; app_id and detail come from
     * the manifest or app behaviour. */
    char app_safe[WLT_SANDBOX_MAX_NAME];
    char detail_safe[128];
    copy_bounded(app_safe, sizeof(app_safe), app_id ? app_id : "-");
    copy_bounded(detail_safe, sizeof(detail_safe), detail ? detail : "");
    sanitise(app_safe, sizeof(app_safe));
    sanitise(detail_safe, sizeof(detail_safe));

    int n = snprintf(line, sizeof(line),
                     "%s %-7s app=%-32s pid=%-7d event=%-12s %s\n",
                     ts, level_str(level), app_safe, (int)pid,
                     event ? event : "-", detail_safe);
    if (n < 0)
        return;
    if ((size_t)n >= sizeof(line))
        n = (int)sizeof(line) - 1;  /* truncated, but still valid */

    /* Single write — atomic up to PIPE_BUF (4096 on Linux). Our lines
     * are well under that, so partial writes shouldn't happen. */
    ssize_t wrote = write(g_audit_fd, line, (size_t)n);
    if (wrote < 0)
    {
        /* EINTR is recoverable; other errors we just log and move on.
         * We never propagate the failure up — losing an audit line is
         * better than killing a sandboxed app over a log write. */
        if (errno != EINTR)
            WLT_AUDIT_LOG_ERR("write audit: %s", strerror(errno));
    }
}
