/**
 * @file service.c
 * @brief Service loading and management
 * @module wlt-init/service
 * @purpose Load, start, stop, restart services
 * @spec: docs/specs/wlt-init.md
 */

#include "service.h"
#include "log.h"
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

wlt_init_error_t
wlt_service_load_all(wlt_init_t *init)
{
    /* TODO(wlt-init): Implement — read all .service files from /etc/wlt/services/ */
    /* For each file: wlt_service_load(path, &init->services[init->service_count++]) */
    (void)init;
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_service_load(const char *path, wlt_service_t *service)
{
    /* TODO(wlt-init): Parse INI-style service file */
    /* Format: */
    /*   [wlt-service] */
    /*   name = WLT Compositor */
    /*   exec = /usr/bin/wlt-compositor */
    /*   args = --config /etc/wlt/compositor.conf */
    /*   depends = tor-vm,key-vault */
    /*   tor_required = yes */
    /*   restart_on_crash = yes */
    /*   priority = critical */
    (void)path;
    (void)service;
    return WLT_INIT_OK;
}

wlt_service_t *
wlt_service_find(wlt_init_t *init, const char *name)
{
    for (size_t i = 0; i < init->service_count; i++)
    {
        if (strcmp(init->services[i].name, name) == 0)
        {
            return &init->services[i];
        }
    }
    return NULL;
}

wlt_init_error_t
wlt_service_start(wlt_init_t *init, wlt_service_t *svc)
{
    /* TODO(wlt-init): Fork + exec the service */
    /* Steps: */
    /*   1. fork() */
    /*   2. In child: set up cgroup, namespace, seccomp (via wlt-sandbox) */
    /*   3. In child: drop privileges to svc->user */
    /*   4. In child: exec(svc->exec, svc->args) */
    /*   5. In parent: record svc->pid, set state to RUNNING */
    (void)init;
    (void)svc;
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_service_stop(wlt_init_t *init, wlt_service_t *svc)
{
    /* TODO(wlt-init): Send SIGTERM, wait, then SIGKILL if needed */
    (void)init;
    (void)svc;
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_service_restart(wlt_init_t *init, wlt_service_t *svc)
{
    wlt_service_stop(init, svc);
    return wlt_service_start(init, svc);
}

const char *
wlt_service_state_str(wlt_service_state_t state)
{
    switch (state)
    {
        case WLT_SVC_STOPPED:  return "stopped";
        case WLT_SVC_STARTING: return "starting";
        case WLT_SVC_RUNNING:  return "running";
        case WLT_SVC_STOPPING: return "stopping";
        case WLT_SVC_FAILED:   return "failed";
        case WLT_SVC_FROZEN:   return "frozen";
        default:               return "unknown";
    }
}
