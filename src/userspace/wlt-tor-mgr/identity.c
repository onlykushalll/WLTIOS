/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wlt-tor-mgr: Identity Rotation
 *
 * "New identity" = drop all existing circuits and force Tor to build fresh
 * ones over a new path (SIGNAL NEWNYM), then clear the application cookie jar
 * so browser-style correlation across the rotation is impossible.
 *
 * Tor rate-limits NEWNYM to one per 10s by default; we surface that as a soft
 * error rather than silently no-op'ing.
 *
 * Security: no identifying data is logged.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "tor_mgr.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

int wlt_tor_new_identity(wlt_tor_control_t *ctl)
{
    int rc;
    if (!ctl)
        return -EINVAL;

    rc = wlt_tor_control_signal(ctl, "NEWNYM");
    if (rc < 0) {
        wlt_log_wrn("NEWNYM failed: %s", strerror(-rc));
        return rc;
    }
    wlt_log_inf("identity rotated (NEWNYM sent)");

    /* Clear the application cookie jar to break session linkage. We tolerate
     * the directory not existing. */
    (void)wlt_tor_clear_cookies(WLT_TOR_COOKIE_JAR);
    return 0;
}

static bool cookie_file_name(const char *name)
{
    /* Match the common browser/HTTP cookie-store file names. */
    static const char *const names[] = {
        "cookies.sqlite", "cookies.sqlite-shm", "cookies.sqlite-wal",
        "cookies.txt", "Cookies", "Cookies-journal",
        ".cookies", "cookies", NULL
    };
    for (size_t i = 0; names[i]; i++)
        if (strcmp(name, names[i]) == 0)
            return true;
    /* Anything ending in ".cookies" is treated as a cookie store too. */
    size_t n = strlen(name);
    if (n > 8 && strcmp(name + n - 8, ".cookies") == 0)
        return true;
    return false;
}

int wlt_tor_clear_cookies(const char *cookie_jar_dir)
{
    DIR *d;
    struct dirent *e;
    int cleared = 0;

    if (!cookie_jar_dir || !*cookie_jar_dir)
        return -EINVAL;

    d = opendir(cookie_jar_dir);
    if (!d) {
        if (errno == ENOENT)
            return 0; /* nothing to clear */
        wlt_log_dbg("opendir(%s): %s", cookie_jar_dir, strerror(errno));
        return -errno;
    }
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        if (!cookie_file_name(e->d_name))
            continue;
        char path[1024];
        if (snprintf(path, sizeof(path), "%s/%s", cookie_jar_dir,
                     e->d_name) >= (int)sizeof(path))
            continue;
        if (unlink(path) == 0) {
            cleared++;
            wlt_log_dbg("cleared cookie store %s", path);
        } else {
            wlt_log_dbg("unlink(%s): %s", path, strerror(errno));
        }
    }
    closedir(d);
    wlt_log_inf("cleared %d cookie store(s) from %s", cleared, cookie_jar_dir);
    return cleared;
}
