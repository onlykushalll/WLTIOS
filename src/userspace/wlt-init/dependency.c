/**
 * @file dependency.c
 * @brief Dependency resolution (topological sort)
 * @module wlt-init/dependency
 * @purpose Determine startup order based on dependencies
 * @spec: docs/specs/wlt-init.md
 */

#include "dependency.h"
#include "log.h"
#include <string.h>
#include <stdbool.h>

wlt_init_error_t
wlt_dependency_resolve(const wlt_init_t *init,
                        size_t order[],
                        size_t *count)
{
    /* TODO(wlt-init): Implement topological sort */
    /* Algorithm: */
    /*   1. Build dependency graph */
    /*   2. Topological sort (Kahn's algorithm) */
    /*   3. Output order[] */
    (void)init;
    (void)order;
    *count = 0;
    return WLT_INIT_OK;
}

bool
wlt_dependency_deps_running(const wlt_init_t *init,
                             const wlt_service_t *svc)
{
    for (size_t i = 0; i < svc->depend_count; i++)
    {
        wlt_service_t *dep = wlt_service_find((wlt_init_t *)init, svc->depends[i]);
        if (dep == NULL || dep->state != WLT_SVC_RUNNING)
        {
            return false;
        }
    }
    return true;
}

bool
wlt_dependency_has_cycle(const wlt_init_t *init)
{
    /* TODO(wlt-init): Implement cycle detection (DFS) */
    (void)init;
    return false;
}
