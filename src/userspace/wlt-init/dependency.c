/**
 * @file dependency.c
 * @brief Dependency resolution — topological sort (Kahn's algorithm)
 * @module wlt-init/dependency
 * @purpose Determines correct service startup order based on dependencies
 * @dependencies: dependency.h, init.h, service.h, log.h
 * @tested_by: tests/userspace/dependency_test.c
 * @spec: docs/specs/wlt-init.md
 */

#include "dependency.h"
#include "service.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* ─── Internal: Find service index by name ───────────── */

static int
find_service_index(const wlt_init_t *init, const char *name)
{
    for (size_t i = 0; i < init->service_count; i++)
    {
        if (strcmp(init->services[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;  /* Not found */
}

/* ─── Public API ─────────────────────────────────────── */

wlt_init_error_t
wlt_dependency_resolve(const wlt_init_t *init,
                        size_t order[],
                        size_t *count)
{
    if (init == NULL || order == NULL || count == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    size_t n = init->service_count;
    *count = 0;

    if (n == 0)
        return WLT_INIT_OK;

    if (n > WLT_INIT_MAX_SERVICES)
        return WLT_INIT_ERR_INVALID_ARG;

    /* Build in-degree array (how many deps each service has) */
    int in_degree[WLT_INIT_MAX_SERVICES];
    memset(in_degree, 0, sizeof(in_degree));

    for (size_t i = 0; i < n; i++)
    {
        in_degree[i] = (int)init->services[i].depend_count;
    }

    /* Kahn's algorithm: BFS from nodes with no dependencies */
    /* Queue of nodes with in_degree == 0 */
    int queue[WLT_INIT_MAX_SERVICES];
    int q_head = 0, q_tail = 0;

    /* Find all services with no dependencies → add to queue */
    for (size_t i = 0; i < n; i++)
    {
        if (in_degree[i] == 0)
        {
            queue[q_tail++] = (int)i;
        }
    }

    /* Process queue */
    while (q_head < q_tail)
    {
        int idx = queue[q_head++];
        order[(*count)++] = (size_t)idx;

        /* For each service that depends on this one, reduce in_degree */
        for (size_t j = 0; j < n; j++)
        {
            if (in_degree[j] > 0)
            {
                /* Check if service j depends on service idx */
                const wlt_service_t *svc = &init->services[j];
                for (size_t d = 0; d < svc->depend_count; d++)
                {
                    if (strcmp(svc->depends[d],
                               init->services[idx].name) == 0)
                    {
                        in_degree[j]--;
                        if (in_degree[j] == 0)
                        {
                            queue[q_tail++] = (int)j;
                        }
                        break;  /* Only count each dependency once */
                    }
                }
            }
        }
    }

    /* Check for cycle (not all nodes processed) */
    if (*count < n)
    {
        WLT_LOG_ERR("Circular dependency detected! "
                     "Resolved %zu of %zu services", *count, n);

        /* Log which services are in the cycle */
        for (size_t i = 0; i < n; i++)
        {
            if (in_degree[i] > 0)
            {
                WLT_LOG_ERR("  Stuck: %s (in_degree=%d)",
                             init->services[i].name, in_degree[i]);
            }
        }

        return WLT_INIT_ERR_INVALID_ARG;
    }

    WLT_LOG_DBG("Dependency resolution: %zu services ordered", *count);
    return WLT_INIT_OK;
}

bool
wlt_dependency_deps_running(const wlt_init_t *init,
                             const wlt_service_t *svc)
{
    if (init == NULL || svc == NULL)
        return false;

    for (size_t i = 0; i < svc->depend_count; i++)
    {
        wlt_service_t *dep = wlt_service_find((wlt_init_t *)init,
                                               svc->depends[i]);
        if (dep == NULL)
        {
            WLT_LOG_WRN("Dependency not found: %s (for %s)",
                         svc->depends[i], svc->name);
            return false;
        }
        if (dep->state != WLT_SVC_RUNNING)
        {
            return false;
        }
    }
    return true;
}

bool
wlt_dependency_has_cycle(const wlt_init_t *init)
{
    if (init == NULL || init->service_count == 0)
        return false;

    size_t order[WLT_INIT_MAX_SERVICES];
    size_t count = 0;

    wlt_init_error_t err = wlt_dependency_resolve(init, order, &count);
    if (err != WLT_INIT_OK)
        return true;  /* Resolve failed = cycle */

    return count < init->service_count;
}
