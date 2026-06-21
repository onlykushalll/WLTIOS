/**
 * @file dependency.h
 * @brief Dependency resolution for WLT-Init
 * @module wlt-init/dependency
 * @purpose Resolves service dependencies, determines startup order
 * @dependencies: init.h
 * @spec: docs/specs/wlt-init.md
 */

#ifndef WLT_DEPENDENCY_H
#define WLT_DEPENDENCY_H

#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve dependencies and sort services in startup order
 * @param init       Init state (with all services loaded)
 * @param order      Output: array of service indices in startup order
 * @param count      Output: number of services in order
 * @return WLT_INIT_OK on success, error if circular dependency
 */
wlt_init_error_t wlt_dependency_resolve(const wlt_init_t *init,
                                          size_t order[],
                                          size_t *count);

/**
 * Check if a service's dependencies are all running
 * @return true if all deps are running
 */
bool wlt_dependency_deps_running(const wlt_init_t *init,
                                  const wlt_service_t *svc);

/**
 * Detect circular dependencies
 * @return true if circular dependency found
 */
bool wlt_dependency_has_cycle(const wlt_init_t *init);

#ifdef __cplusplus
}
#endif

#endif /* WLT_DEPENDENCY_H */
