/**
 * @file service.h
 * @brief Service loading and parsing for WLT-Init
 * @module wlt-init/service
 * @purpose Loads service definitions from /etc/wlt/services/
 * @dependencies: init.h
 * @spec: docs/specs/wlt-init.md
 */

#ifndef WLT_SERVICE_H
#define WLT_SERVICE_H

#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load all service definitions from /etc/wlt/services/
 * @param init  Init state to populate
 * @return WLT_INIT_OK on success
 */
wlt_init_error_t wlt_service_load_all(wlt_init_t *init);

/**
 * Load a single service definition
 * @param path      Service file path
 * @param service   Output service struct
 * @return WLT_INIT_OK on success
 */
wlt_init_error_t wlt_service_load(const char *path, wlt_service_t *service);

/**
 * Find a service by name
 * @return Pointer to service, or NULL if not found
 */
wlt_service_t *wlt_service_find(wlt_init_t *init, const char *name);

/**
 * Start a service
 */
wlt_init_error_t wlt_service_start(wlt_init_t *init, wlt_service_t *svc);

/**
 * Stop a service
 */
wlt_init_error_t wlt_service_stop(wlt_init_t *init, wlt_service_t *svc);

/**
 * Restart a service
 */
wlt_init_error_t wlt_service_restart(wlt_init_t *init, wlt_service_t *svc);

/**
 * Get state string
 */
const char *wlt_service_state_str(wlt_service_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* WLT_SERVICE_H */
