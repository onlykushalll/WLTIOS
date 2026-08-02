/*
 * firewall_pd.c — Firewall Protection Domain implementation
 *
 * B.2: Real IPC proof (ping/pong) + basic packet-filtering logic.
 *
 * This PD runs as a bare seL4 protection domain (no Linux, no libc).
 * It communicates with the Linux VM via shared memory + notifications.
 *
 * IPC Protocol:
 *   VM writes "PING" to src_dp, signals ready
 *   PD reads src_dp, writes "PONG" to dest_dp, signals done
 *   This proves the VM-to-PD data path works.
 *
 * Packet Filtering (B.3):
 *   The PD evaluates packets against compiled-in rules:
 *   - Allow: Tor VM -> NIC (Tor traffic exits)
 *   - Deny: Main Linux -> NIC (direct, must go through Tor)
 *   - Deny: DNS not to Tor DNS port (prevent DNS leak)
 *   - Kill switch: Deny ALL
 */
#include <camkes.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ─── Firewall Rules (compiled into PD, cannot be modified at runtime) ─── */
typedef enum {
    FW_ACTION_ALLOW = 0,
    FW_ACTION_DENY  = 1,
} fw_action_t;

typedef enum {
    FW_REASON_NONE         = 0,
    FW_REASON_DIRECT_TRAFFIC = 1,  /* Main Linux -> NIC directly */
    FW_REASON_DNS_LEAK     = 2,   /* DNS not through Tor */
    FW_REASON_KILL_SWITCH  = 3,   /* Kill switch active */
    FW_REASON_DEFAULT_DENY = 4,   /* No rule matched */
} fw_reason_t;

typedef struct {
    fw_action_t action;
    fw_reason_t reason;
    uint32_t packet_count;
    uint32_t drop_count;
} fw_state_t;

static fw_state_t g_fw_state;
static bool g_kill_switch = false;

/* ─── Packet filtering logic ─── */
static fw_action_t
fw_evaluate_packet(const uint8_t *data, size_t len)
{
    if (g_kill_switch) {
        g_fw_state.reason = FW_REASON_KILL_SWITCH;
        return FW_ACTION_DENY;
    }

    /* Minimal packet inspection: check if this looks like a DNS packet
     * (UDP port 53). In a real implementation, this would parse full
     * IP/TCP/UDP headers. For the IPC proof, we just check the data. */
    if (len >= 4 && data[0] == 'D' && data[1] == 'N' && data[2] == 'S') {
        /* DNS packet — deny unless going to Tor DNS port */
        g_fw_state.reason = FW_REASON_DNS_LEAK;
        return FW_ACTION_DENY;
    }

    /* Check if this is direct traffic (not through Tor) */
    if (len >= 4 && data[0] == 'D' && data[1] == 'I' && data[2] == 'R') {
        /* Direct traffic — deny */
        g_fw_state.reason = FW_REASON_DIRECT_TRAFFIC;
        return FW_ACTION_DENY;
    }

    /* Default: allow (Tor traffic passes) */
    g_fw_state.reason = FW_REASON_NONE;
    return FW_ACTION_ALLOW;
}

/* ─── IPC handler: process data from VM ─── */
static void
fw_process_ipc(void)
{
    uint8_t *src = (uint8_t *)src_dp;
    uint8_t *dest = (uint8_t *)dest_dp;

    /* Read command from VM */
    g_fw_state.packet_count++;

    /* Check for PING (IPC proof) */
    if (memcmp(src, "PING", 4) == 0) {
        /* Write PONG response */
        memcpy(dest, "PONG", 4);
        printf("[firewall-pd] PING->PONG (IPC proof OK)\n");
        return;
    }

    /* Check for KILL switch command */
    if (memcmp(src, "KILL", 4) == 0) {
        g_kill_switch = true;
        memcpy(dest, "KILLED", 6);
        printf("[firewall-pd] Kill switch ACTIVATED\n");
        return;
    }

    /* Check for KILL clear command */
    if (memcmp(src, "ALIVE", 5) == 0) {
        g_kill_switch = false;
        memcpy(dest, "ALIVE-OK", 8);
        printf("[firewall-pd] Kill switch CLEARED\n");
        return;
    }

    /* Check for STATUS command */
    if (memcmp(src, "STAT", 4) == 0) {
        /* Return status: packets, drops, kill switch state */
        dest[0] = (g_fw_state.packet_count >> 24) & 0xFF;
        dest[1] = (g_fw_state.packet_count >> 16) & 0xFF;
        dest[2] = (g_fw_state.packet_count >> 8) & 0xFF;
        dest[3] = g_fw_state.packet_count & 0xFF;
        dest[4] = (g_fw_state.drop_count >> 24) & 0xFF;
        dest[5] = (g_fw_state.drop_count >> 16) & 0xFF;
        dest[6] = (g_fw_state.drop_count >> 8) & 0xFF;
        dest[7] = g_fw_state.drop_count & 0xFF;
        dest[8] = g_kill_switch ? 1 : 0;
        printf("[firewall-pd] Status: pkts=%u drops=%u kill=%d\n",
               g_fw_state.packet_count, g_fw_state.drop_count, g_kill_switch);
        return;
    }

    /* Otherwise: evaluate as packet */
    fw_action_t action = fw_evaluate_packet(src, 4096);
    if (action == FW_ACTION_DENY) {
        g_fw_state.drop_count++;
        memcpy(dest, "DROP", 4);
        printf("[firewall-pd] DROP (reason=%d)\n", g_fw_state.reason);
    } else {
        memcpy(dest, "ALLOW", 5);
        printf("[firewall-pd] ALLOW\n");
    }
}

/* ─── CAmkES entry points ─── */
void run(void)
{
    printf("[firewall-pd] Starting (bare seL4 PD, camkes-based)\n");
    printf("[firewall-pd] Rules: allow Tor, deny direct, deny DNS leak\n");
    g_fw_state.packet_count = 0;
    g_fw_state.drop_count = 0;
    g_kill_switch = false;

    /* Main loop: wait for VM signals, process data */
    while (1) {
        /* Wait for VM to signal data is ready */
        ready_wait();

        /* Process the IPC/packet */
        fw_process_ipc();

        /* Signal VM that response is ready */
        done_emit();
    }
}
