/**
 * @file init.h
 * @brief Public interface for WLT-Init (PID 1)
 * @module wlt-init
 * @purpose First userspace process — starts all services, monitors them
 * @dependencies: service.h, monitor.h, dependency.h, tor.h, mount.h, log.h
 * @tested_by: tests/userspace/init_test.c
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_INIT_H
#define WLT_INIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_INIT_OK = 0,
    WLT_INIT_ERR_INVALID_ARG = -1,
    WLT_INIT_ERR_NOT_FOUND = -2,
    WLT_INIT_ERR_IO = -3,
    WLT_INIT_ERR_NOMEM = -4,
    WLT_INIT_ERR_PERMISSION = -5,
    WLT_INIT_ERR_TIMEOUT = -6,
    WLT_INIT_ERR_TOR_NOT_READY = -7,
    WLT_INIT_ERR_KEYVAULT = -8,
} wlt_init_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_INIT_VERSION "1.0.0"
#define WLT_INIT_MAX_SERVICES 64
#define WLT_INIT_MAX_NAME 64
#define WLT_INIT_MAX_PATH 256
#define WLT_INIT_MAX_ARGS 512
#define WLT_INIT_SERVICE_DIR "/etc/wlt/services"
#define WLT_INIT_LOG_DIR "/var/log/wlt"
#define WLT_INIT_RESTART_BACKOFF_MS 2000

/* ─── Types ──────────────────────────────────────────── */

typedef enum {
    WLT_SVC_STOPPED = 0,
    WLT_SVC_STARTING,
    WLT_SVC_RUNNING,
    WLT_SVC_STOPPING,
    WLT_SVC_FAILED,
    WLT_SVC_FROZEN,  /* Frozen due to Tor disconnect (kill switch) */
} wlt_service_state_t;

typedef enum {
    WLT_PRIO_CRITICAL = 0,  /* Never give up — restart immediately */
    WLT_PRIO_NORMAL = 1,    /* Restart with backoff */
    WLT_PRIO_LOW = 2,       /* Don't restart automatically */
} wlt_service_priority_t;

typedef struct {
    char name[WLT_INIT_MAX_NAME];
    char exec[WLT_INIT_MAX_PATH];
    char args[WLT_INIT_MAX_ARGS];
    char depends[8][WLT_INIT_MAX_NAME];  /* Service dependencies */
    size_t depend_count;
    bool tor_required;       /* Don't start until Tor is ready */
    bool restart_on_crash;
    uint32_t restart_backoff_ms;
    wlt_service_priority_t priority;
    char user[32];           /* Run as this user */
    /* Runtime state */
    wlt_service_state_t state;
    pid_t pid;
    uint64_t started_at;
    uint32_t restart_count;
} wlt_service_t;

typedef struct {
    wlt_service_t services[WLT_INIT_MAX_SERVICES];
    size_t service_count;
    bool tor_ready;
    bool kill_switch_active;
} wlt_init_t;

/* ─── Public API ─────────────────────────────────────── */

/**
 * Init entry point (called by Linux kernel as PID 1)
 * @return Should never return (init runs forever)
 */
int main(void);

/**
 * Get error string
 */
const char *wlt_init_error_str(wlt_init_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_INIT_H */
