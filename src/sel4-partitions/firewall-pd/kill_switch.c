/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * WLTIOS - seL4 Firewall Protection Domain
 * kill_switch.c
 *
 * The network kill switch.
 *
 * When armed, the firewall drops EVERY packet - regardless of source,
 * destination or rule. This is the "panic button" used by WLT-Tor-Mgr
 * (and, on unrecoverable faults, by the firewall itself) to guarantee
 * that no traffic leaves the device.
 *
 * Design:
 *   - State is a single word in firewall-owned memory, modified only
 *     by protected procedure calls (from WLT-Tor-Mgr) or by the
 *     firewall itself. Linux guests have no handle to it.
 *   - All updates use release barriers so the rule engine (which
 *     reads the flag on every packet) observes the change promptly.
 *     Target trigger latency is < 100 us (spec).
 *   - The switch is FAIL-CLOSED on IPC failure: if the WLT-Tor-Mgr
 *     channel ever errors, the switch arms and stays armed. Clearing
 *     it requires an explicit, successful IPC.
 *   - On activate, the firewall also notifies WLT-Init (via the
 *     WLT-Tor-Mgr channel) so the rest of the system learns the
 *     network is down.
 */
#include "firewall.h"

/* ------------------------------------------------------------------ */
/* Switch state.                                                       */
/*                                                                      */
/* We keep a small structure rather than a bare flag so that the       */
/* reason and timestamp of the last transition are available to the    */
/* status IPC. The whole structure is owned by this PD.                */
/* ------------------------------------------------------------------ */
typedef struct kill_switch_state {
    volatile uint32_t active;       /* 0 = inactive, 1 = armed        */
    volatile uint32_t fail_closed;  /* 1 = latched (only clear via    */
                                    /*       successful IPC)          */
    volatile uint64_t transitions;  /* total arm/clear transitions    */
    volatile uint32_t last_reason;  /* why it was last armed          */
} kill_switch_state_t;

/* Reason codes for arming the switch (distinct from firewall_reason_t
 * so we can tell "armed by user" from "armed by fault"). */
#define FW_KILL_REASON_USER       1   /* WLT-Tor-Mgr requested       */
#define FW_KILL_REASON_NIC_DEAD   2   /* NIC PD unresponsive         */
#define FW_KILL_REASON_IPC_FAIL   3   /* control IPC failed          */
#define FW_KILL_REASON_INTERNAL   4   /* other internal fault         */

static kill_switch_state_t g_kill;

/* ------------------------------------------------------------------ */
/* Barrier helpers (ARMv8).                                            */
/* ------------------------------------------------------------------ */
static inline void fw_release_barrier(void)
{
    __asm__ volatile("dmb ishst" ::: "memory");
}

static inline void fw_acquire_barrier(void)
{
    __asm__ volatile("dmb ishld" ::: "memory");
}

/* ------------------------------------------------------------------ */
/* Public API.                                                         */
/* ------------------------------------------------------------------ */
bool kill_switch_is_active(void)
{
    /* Acquire barrier pairs with the release in activate(). */
    uint32_t v = g_kill.active;
    fw_acquire_barrier();
    return v != 0;
}

bool kill_switch_activate(void)
{
    /* Volatile read-modify-write. seL4 PDs are single-threaded so we
     * do not need an atomic CAS - we just need the store to be
     * ordered before any subsequent packet read. */
    bool was = (g_kill.active != 0);
    g_kill.last_reason = FW_KILL_REASON_USER;
    g_kill.active = 1;
    fw_release_barrier();
    if (!was) {
        g_kill.transitions++;
    }

    /* Notify WLT-Init (through WLT-Tor-Mgr) that the network is down.
     * We use a notification rather than a PPC so this never blocks. */
    microkit_notify(FW_CH_WLT_TOR_MGR);
    return was;
}

bool kill_switch_deactivate(void)
{
    /* Only a successful IPC from WLT-Tor-Mgr may clear the switch.
     * If the fail-closed latch is set, we keep the switch armed. */
    if (g_kill.fail_closed) {
        return true;            /* still armed */
    }

    bool was = (g_kill.active != 0);
    g_kill.active = 0;
    fw_release_barrier();
    if (was) {
        g_kill.transitions++;
    }
    return was;
}

void kill_switch_fail_closed(void)
{
    /* Latch the fault and arm the switch. Once latched, only a reboot
     * (or an explicit privileged recovery path) can clear it. */
    g_kill.fail_closed = 1;
    g_kill.last_reason = FW_KILL_REASON_INTERNAL;
    g_kill.active = 1;
    fw_release_barrier();
    g_kill.transitions++;

    /* Best-effort notification. If this fails we are still armed,
     * which is the correct (fail-closed) state. */
    microkit_notify(FW_CH_WLT_TOR_MGR);
}

/* ------------------------------------------------------------------ */
/* Internal entry points used by firewall.c on fault paths.            */
/* ------------------------------------------------------------------ */

/* Called when the NIC driver PD appears unresponsive. Latches the
 * switch so it cannot be cleared by a normal IPC. */
void kill_switch_nic_dead(void)
{
    g_kill.fail_closed = 1;
    g_kill.last_reason = FW_KILL_REASON_NIC_DEAD;
    g_kill.active = 1;
    fw_release_barrier();
    g_kill.transitions++;
    microkit_notify(FW_CH_WLT_TOR_MGR);
}

/* Called when a control IPC to WLT-Tor-Mgr fails. Same fail-closed
 * behaviour. */
void kill_switch_ipc_failed(void)
{
    g_kill.fail_closed = 1;
    g_kill.last_reason = FW_KILL_REASON_IPC_FAIL;
    g_kill.active = 1;
    fw_release_barrier();
    g_kill.transitions++;
}

/* Read the latched fault flag (for status reporting). */
bool kill_switch_fail_closed_latched(void)
{
    return g_kill.fail_closed != 0;
}

/* Read the last arm reason (for status reporting). */
uint32_t kill_switch_last_reason(void)
{
    return g_kill.last_reason;
}

/* Total arm/clear transitions (for status reporting). */
uint64_t kill_switch_transitions(void)
{
    return g_kill.transitions;
}
