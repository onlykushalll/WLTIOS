/**
 * @file monitor.h
 * @brief Process monitoring for WLT-Init
 * @module wlt-init/monitor
 * @purpose Monitors running services, restarts crashed ones
 * @dependencies: init.h, service.h
 * @spec: docs/specs/wlt-init.md
 */

#ifndef WLT_MONITOR_H
#define WLT_MONITOR_H

#include "init.h"
#include "service.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Monitor loop — waits for child processes, handles crashes
 * Runs forever (until shutdown)
 */
wlt_init_error_t wlt_monitor_loop(wlt_init_t *init);

/**
 * Handle a crashed service (restart if configured)
 */
void wlt_monitor_handle_crash(wlt_init_t *init, wlt_service_t *svc, int exit_code);

/**
 * Check if service should be restarted
 */
bool wlt_monitor_should_restart(const wlt_service_t *svc, int exit_code);

#ifdef __cplusplus
}
#endif

#endif /* WLT_MONITOR_H */
