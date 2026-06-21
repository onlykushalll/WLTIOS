/**
 * @file tor.c
 * @brief Tor status tracking
 * @module wlt-init/tor
 * @purpose Track Tor readiness, manage kill switch
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 */

#include "tor.h"
#include "service.h"
#include "log.h"
#include <unistd.h>

wlt_init_error_t
wlt_tor_init(wlt_init_t *init)
{
    init->tor_ready = false;
    init->kill_switch_active = false;
    return WLT_INIT_OK;
}

bool
wlt_tor_is_ready(const wlt_init_t *init)
{
    return init->tor_ready;
}

wlt_init_error_t
wlt_tor_wait_ready(wlt_init_t *init, uint32_t timeout_ms)
{
    /* TODO(wlt-init): Poll Tor status via IPC to Firewall PD */
    uint32_t elapsed = 0;
    while (!wlt_tor_is_ready(init) && elapsed < timeout_ms)
    {
        usleep(100000);  /* 100ms */
        elapsed += 100;
    }
    if (wlt_tor_is_ready(init))
        return WLT_INIT_OK;
    return WLT_INIT_ERR_TIMEOUT;
}

void
wlt_tor_on_disconnect(wlt_init_t *init)
{
    WLT_LOG_WRN("Tor disconnected — activating kill switch");
    init->tor_ready = false;
    init->kill_switch_active = true;

    /* Freeze all network-dependent services */
    for (size_t i = 0; i < init->service_count; i++)
    {
        wlt_service_t *svc = &init->services[i];
        if (svc->tor_required && svc->state == WLT_SVC_RUNNING)
        {
            WLT_LOG_INF("Freezing %s (Tor disconnected)", svc->name);
            /* TODO(wlt-init): Send SIGSTOP to freeze (not kill) */
            svc->state = WLT_SVC_FROZEN;
        }
    }
}

void
wlt_tor_on_reconnect(wlt_init_t *init)
{
    WLT_LOG_INF("Tor reconnected — unfreezing services");
    init->tor_ready = true;
    init->kill_switch_active = false;

    /* Unfreeze all frozen services */
    for (size_t i = 0; i < init->service_count; i++)
    {
        wlt_service_t *svc = &init->services[i];
        if (svc->state == WLT_SVC_FROZEN)
        {
            WLT_LOG_INF("Unfreezing %s", svc->name);
            /* TODO(wlt-init): Send SIGCONT to resume */
            svc->state = WLT_SVC_RUNNING;
        }
    }
}

void
wlt_tor_kill_switch(wlt_init_t *init)
{
    wlt_tor_on_disconnect(init);
}
