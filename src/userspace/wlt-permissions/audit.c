/**
 * @file audit.c
 * @brief Append-only security audit log for WLT-Permissions
 * @module wlt-permissions/audit
 * @purpose Records who granted/revoked/denied what permission and when,
 *          for forensics and the spec Â§Tests audit-log test.
 * @dependencies: manifest.h, permissions.h
 * @tested_by: tests/userspace/permissions_audit_test.c
 * @spec: docs/specs/wlt-permissions.md
 *
 * The log lives at WLT_PERMS_AUDIT_LOG (/var/log/wlt/permissions.log).
 * It is opened O_APPEND|O_CLOEXEC so the launcher forked by WLT-Sandbox
 * does not inherit the fd, and concurrent writers stay atomic up to
 * PIPE_BUF (4096 on Linux) â our lines are well under that. A process-
 * wide singleton fd is safe because the permission daemon is single
 * threaded; the shell talks to it over IPC rather than sharing the fd.
 */

#include "permissions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

/* The audit module is the lowest layer â it must not call back into the
 * manager, so diagnostics go straight to stderr. */
#define WLT_PA_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-permissions/audit][ERR] " fmt "\n", ##__VA_ARGS__)

static int g_audit_fd = -1;

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

/* Format the current UTC time as ISO 8601 (2026-06-22T13:01:07Z). */
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

/* Replace newlines/tabs with spaces so an attacker cannot inject fake
 * log lines via a crafted app id or detail string. */
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

/* âââ Public API âââââââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_audit_init(void)
{
    if (g_audit_fd >= 0)
        return WLT_PERMS_OK;  /* already open */

    /* 0750: root + wlt group can read the log, others cannot. */
    if (mkdir(WLT_PERMS_LOG_DIR, 0750) != 0 && errno != EEXIST)
    {
        WLT_PA_LOG_ERR("mkdir(%s): %s", WLT_PERMS_LOG_DIR, strerror(errno));
        return WLT_PERMS_ERR_IO;
    }

    g_audit_fd = open(WLT_PERMS_AUDIT_LOG,
                      O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                      0640);
    if (g_audit_fd < 0)
    {
        WLT_PA_LOG_ERR("open(%s): %s",
                       WLT_PERMS_AUDIT_LOG, strerror(errno));
        return WLT_PERMS_ERR_IO;
    }

    /* Separator line so readers can spot daemon restarts. */
    const char *boot = "# wlt-permissions audit log opened\n";
    ssize_t n = write(g_audit_fd, boot, strlen(boot));
    (void)n;
    return WLT_PERMS_OK;
}

void
wlt_perms_audit_close(void)
{
    if (g_audit_fd >= 0)
    {
        close(g_audit_fd);
        g_audit_fd = -1;
    }
}

void
wlt_perms_audit_log(wlt_perms_audit_level_t level, const char *app_id,
                    wlt_perms_id_t perm, const char *action,
                    const char *by, const char *detail)
{
    if (g_audit_fd < 0)
    {
        /* Lazy-init so callers need not check return values. If init
         * fails we drop the event silently â better than crashing the
         * permission daemon over a log write. */
        if (wlt_perms_audit_init() != WLT_PERMS_OK)
            return;
    }

    char ts[32];
    if (format_iso8601_utc((uint64_t)time(NULL), ts, sizeof(ts)) != 0)
        copy_bounded(ts, sizeof(ts), "?");

    /* Sanitise user-controlled fields. level/action are our own
     * constants; app_id/by/detail may originate from manifests or IPC. */
    char app_safe[WLT_PERMS_MAX_NAME];
    char by_safe[WLT_PERMS_MAX_BY];
    char detail_safe[WLT_PERMS_MAX_DETAIL];
    copy_bounded(app_safe, sizeof(app_safe), app_id ? app_id : "-");
    copy_bounded(by_safe, sizeof(by_safe), by ? by : "-");
    copy_bounded(detail_safe, sizeof(detail_safe), detail ? detail : "");
    sanitise(app_safe, sizeof(app_safe));
    sanitise(by_safe, sizeof(by_safe));
    sanitise(detail_safe, sizeof(detail_safe));

    char line[512];
    int n = snprintf(line, sizeof(line),
                     "%s %-8s app=%-32s perm=%-12s action=%-16s by=%-16s %s\n",
                     ts,
                     wlt_perms_audit_level_str(level),
                     app_safe,
                     wlt_perms_id_str(perm),
                     action ? action : "-",
                     by_safe,
                     detail_safe);
    if (n < 0)
        return;
    if ((size_t)n >= sizeof(line))
        n = (int)sizeof(line) - 1;  /* truncated, still a valid line */

    ssize_t wrote = write(g_audit_fd, line, (size_t)n);
    if (wrote < 0 && errno != EINTR)
        WLT_PA_LOG_ERR("write audit: %s", strerror(errno));
}
