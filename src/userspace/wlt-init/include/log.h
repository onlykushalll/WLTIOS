/**
 * @file log.h
 * @brief Logging for WLT-Init
 * @module wlt-init/log
 * @purpose Logs to /var/log/wlt/init.log
 * @dependencies: init.h
 * @spec: docs/specs/wlt-init.md
 */

#ifndef WLT_LOG_H
#define WLT_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_LOG_DEBUG = 0,
    WLT_LOG_INFO = 1,
    WLT_LOG_WARN = 2,
    WLT_LOG_ERROR = 3,
    WLT_LOG_FATAL = 4,
} wlt_log_level_t;

/**
 * Initialize logging (open log file)
 */
void wlt_log_init(const char *path);

/**
 * Close log file
 */
void wlt_log_close(void);

/**
 * Log a message
 */
void wlt_log(wlt_log_level_t level, const char *fmt, ...);

/* Convenience macros */
#define WLT_LOG_DBG(fmt, ...)  wlt_log(WLT_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define WLT_LOG_INF(fmt, ...)  wlt_log(WLT_LOG_INFO,  fmt, ##__VA_ARGS__)
#define WLT_LOG_WRN(fmt, ...)  wlt_log(WLT_LOG_WARN,  fmt, ##__VA_ARGS__)
#define WLT_LOG_ERR(fmt, ...)  wlt_log(WLT_LOG_ERROR, fmt, ##__VA_ARGS__)
#define WLT_LOG_FTL(fmt, ...)  wlt_log(WLT_LOG_FATAL, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* WLT_LOG_H */
