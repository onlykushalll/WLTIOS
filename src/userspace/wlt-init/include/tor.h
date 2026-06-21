/**
 * @file tor.h
 * @brief Tor status tracking for WLT-Init
 * @module wlt-init/tor
 * @purpose Tracks Tor readiness, manages kill switch
 * @dependencies: init.h
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 */

#ifndef WLT_TOR_H
#define WLT_TOR_H

#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Tor status tracking
 */
wlt_init_error_t wlt_tor_init(wlt_init_t *init);

/**
 * Check if Tor is ready (connected + circuits built)
 * @return true if Tor is ready
 */
bool wlt_tor_is_ready(const wlt_init_t *init);

/**
 * Wait for Tor to become ready (with timeout)
 * @param timeout_ms  Max time to wait
 * @return WLT_INIT_OK if ready, WLT_INIT_ERR_TIMEOUT if not
 */
wlt_init_error_t wlt_tor_wait_ready(wlt_init_t *init, uint32_t timeout_ms);

/**
 * Called when Tor disconnects — freeze network-dependent services
 */
void wlt_tor_on_disconnect(wlt_init_t *init);

/**
 * Called when Tor reconnects — unfreeze services
 */
void wlt_tor_on_reconnect(wlt_init_t *init);

/**
 * Trigger kill switch manually
 */
void wlt_tor_kill_switch(wlt_init_t *init);

#ifdef __cplusplus
}
#endif

#endif /* WLT_TOR_H */
