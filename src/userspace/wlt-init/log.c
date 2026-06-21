/**
 * @file log.c
 * @brief Logging implementation
 * @module wlt-init/log
 * @purpose Log to /var/log/wlt/init.log
 * @spec: docs/specs/wlt-init.md
 */

#include "log.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

static FILE *g_log_file = NULL;

void
wlt_log_init(const char *path)
{
    g_log_file = fopen(path, "a");
    if (g_log_file == NULL)
    {
        /* Fall back to stderr */
        g_log_file = stderr;
    }
}

void
wlt_log_close(void)
{
    if (g_log_file && g_log_file != stderr)
    {
        fclose(g_log_file);
    }
}

void
wlt_log(wlt_log_level_t level, const char *fmt, ...)
{
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    time_t now;
    struct tm *tm;
    char timebuf[32];

    time(&now);
    tm = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(g_log_file, "[%s] [%s] ", timebuf, level_str[level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);

    fprintf(g_log_file, "\n");
    fflush(g_log_file);
}
