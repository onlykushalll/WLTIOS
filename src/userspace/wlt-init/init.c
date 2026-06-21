/**
 * @file init.c
 * @brief Main entry point for WLT-Init (PID 1)
 * @module wlt-init
 * @purpose First userspace process — boot sequence + monitoring
 * @dependencies: service.h, monitor.h, dependency.h, tor.h, mount.h, log.h
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 */

#include "init.h"
#include "service.h"
#include "monitor.h"
#include "dependency.h"
#include "tor.h"
#include "mount.h"
#include "log.h"
#include <unistd.h>
#include <sys/wait.h>

static wlt_init_t g_init;

int
main(void)
{
    wlt_init_error_t err;
    size_t startup_order[WLT_INIT_MAX_SERVICES];
    size_t startup_count = 0;

    /* 1. Initialize logging */
    wlt_log_init(WLT_INIT_LOG_DIR "/init.log");
    WLT_LOG_INF("WLTIOS Init v%s starting (PID 1)", WLT_INIT_VERSION);

    /* 2. Mount essential filesystems */
    WLT_LOG_INF("Mounting essential filesystems...");
    err = wlt_mount_essential();
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_FTL("Failed to mount essential filesystems: %s",
                     wlt_init_error_str(err));
        /* Drop to recovery shell */
        execl("/bin/sh", "sh", NULL);
        _exit(1);
    }

    /* 3. Load service definitions */
    WLT_LOG_INF("Loading service definitions...");
    err = wlt_service_load_all(&g_init);
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_FTL("Failed to load services: %s", wlt_init_error_str(err));
        _exit(1);
    }
    WLT_LOG_INF("Loaded %zu services", g_init.service_count);

    /* 4. Check for circular dependencies */
    if (wlt_dependency_has_cycle(&g_init))
    {
        WLT_LOG_FTL("Circular dependency detected in services!");
        _exit(1);
    }

    /* 5. Resolve startup order */
    err = wlt_dependency_resolve(&g_init, startup_order, &startup_count);
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_FTL("Failed to resolve dependencies: %s", wlt_init_error_str(err));
        _exit(1);
    }

    /* 6. Initialize Tor tracking */
    wlt_tor_init(&g_init);

    /* 7. Start services in dependency order */
    for (size_t i = 0; i < startup_count; i++)
    {
        wlt_service_t *svc = &g_init.services[startup_order[i]];

        /* If service requires Tor, wait for it */
        if (svc->tor_required && !wlt_tor_is_ready(&g_init))
        {
            WLT_LOG_INF("Waiting for Tor before starting %s...", svc->name);
            wlt_tor_wait_ready(&g_init, 30000);  /* 30s timeout */
        }

        WLT_LOG_INF("Starting %s...", svc->name);
        err = wlt_service_start(&g_init, svc);
        if (err != WLT_INIT_OK)
        {
            WLT_LOG_ERR("Failed to start %s: %s", svc->name, wlt_init_error_str(err));
        }
    }

    WLT_LOG_INF("All services started. Entering monitor loop.");

    /* 8. Enter monitor loop (runs forever) */
    wlt_monitor_loop(&g_init);

    /* Should never reach here */
    WLT_LOG_FTL("Monitor loop exited unexpectedly");
    _exit(1);
}

const char *
wlt_init_error_str(wlt_init_error_t err)
{
    switch (err)
    {
        case WLT_INIT_OK:               return "OK";
        case WLT_INIT_ERR_INVALID_ARG:  return "Invalid argument";
        case WLT_INIT_ERR_NOT_FOUND:    return "Not found";
        case WLT_INIT_ERR_IO:           return "I/O error";
        case WLT_INIT_ERR_NOMEM:        return "Out of memory";
        case WLT_INIT_ERR_PERMISSION:   return "Permission denied";
        case WLT_INIT_ERR_TIMEOUT:      return "Timeout";
        case WLT_INIT_ERR_TOR_NOT_READY:return "Tor not ready";
        case WLT_INIT_ERR_KEYVAULT:     return "Key Vault unavailable";
        default:                        return "Unknown error";
    }
}
