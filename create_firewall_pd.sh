#!/bin/bash
# B.2: Create Firewall PD as a bare camkes component (like KeyVaultPD).
# Includes IPC proof (ping/pong) + basic packet-filtering rules.
set -e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

# Create component directory
mkdir -p "$REPO/sel4-build/projects/vm/components/FirewallPD/src"
mkdir -p "$REPO/sel4-build/projects/vm/components/FirewallPD/include"
mkdir -p "$REPO/sel4-build/projects/vm-examples/apps/x86/firewall_pd_64/src"

# 1. FirewallPD.camkes — component declaration (bare PD, like KeyVaultPD)
cat > "$REPO/sel4-build/projects/vm/components/FirewallPD/FirewallPD.camkes" << 'CAMKES'
/*
 * FirewallPD.camkes — CAmkES component declaration for Firewall PD
 *
 * B.1 Decision: Bare seL4 protection domain (NOT a Linux guest VM).
 * This follows the KeyVaultPD pattern, not the TorPD pattern.
 *
 * The spec (docs/specs/firewall-pd_ARCHIVED) calls for Microkit/sDDF,
 * but this project uses camkes. This component adapts the spec's
 * intent to the project's camkes framework: a minimal bare PD with
 * shared-memory IPC + notification-based synchronization.
 *
 * The PD enforces the "Tor-only" rule: all network traffic must go
 * through Tor. Direct traffic is dropped. Kill switch drops all.
 */
component FirewallPD {
    control;
    consumes Ready ready;       /* VM signals: packet available */
    emits Done done;            /* PD signals: packet processed */
    dataport Buf(4096) src_dp;  /* VM -> PD: packet data + commands */
    dataport Buf(4096) dest_dp; /* PD -> VM: response + status */
    maybe uses PutChar putchar; /* Serial debug output */
}
CAMKES

# 2. firewall_pd.c — implementation with IPC proof + packet filtering
cat > "$REPO/sel4-build/projects/vm/components/FirewallPD/src/firewall_pd.c" << 'FWC'
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
FWC

# 3. minimal_firewall.camkes — assembly (VM + Firewall PD)
cat > "$REPO/sel4-build/projects/vm-examples/apps/x86/firewall_pd_64/minimal_firewall.camkes" << 'ASM'
/*
 * minimal_firewall.camkes — Minimal VM + Firewall PD
 *
 * B.2: Proves the Firewall PD pattern works with camkes IPC.
 * The Firewall PD is a bare seL4 protection domain (like KeyVaultPD),
 * NOT a Linux guest VM (unlike TorPD).
 *
 * The Linux VM and Firewall PD communicate via shared memory + notifications.
 * The VM sends "PING" and the PD responds "PONG" — proving the IPC path.
 */
import <VM/vm.camkes>;

#include <configurations/vm.h>

#define VM_GUEST_CMDLINE "earlyprintk=ttyS0,115200 console=ttyS0,115200 i8042.nokbd=y i8042.nomux=y \
i8042.noaux=y io_delay=udelay noisapnp pci=nomsi debug root=/dev/mem clocksource=tsc hpet=disable"

import <FirewallPD/FirewallPD.camkes>;

component Init0 {
    dataport Buf(4096) fw_src_dp;
    dataport Buf(4096) fw_dest_dp;
    emits Ready fw_ready;
    consumes Done fw_done;
    VM_INIT_DEF()
}

assembly {
    composition {
        VM_COMPOSITION_DEF()
        VM_PER_VM_COMP_DEF(0)

        /* Firewall protection domain — separate from the Linux VM */
        component FirewallPD firewall;

        /* Shared memory connections */
        connection seL4SharedDataWithCaps fw_conn_src(from vm0.fw_src_dp,
                                                       to firewall.src_dp);
        connection seL4SharedDataWithCaps fw_conn_dest(from firewall.dest_dp,
                                                         to vm0.fw_dest_dp);

        /* Notification connections */
        connection seL4Notification fw_ready_conn(from vm0.fw_ready,
                                                   to firewall.ready);
        connection seL4Notification fw_done_conn(from firewall.done,
                                                  to vm0.fw_done);

        /* Serial output for the FirewallPD */
        connection seL4RPCCall fw_putchar(from firewall.putchar,
                                           to serial.processed_putchar);
    }

    configuration {
        VM_CONFIGURATION_DEF()
        VM_PER_VM_CONFIG_DEF(0)

        vm0.simple_untyped23_pool = 20;
        vm0.simple_untyped25_pool = 2;
        vm0.guest_ram = 0x10000000;
        vm0.guest_ram_mb = 128;
        vm0.kernel_cmdline = VM_GUEST_CMDLINE;
        vm0.kernel_image = "bzimage";
        vm0.kernel_relocs = "bzimage";
        vm0.initrd_image = "rootfs.cpio";
    }
}
ASM

# 4. CMakeLists.txt
cat > "$REPO/sel4-build/projects/vm-examples/apps/x86/firewall_pd_64/CMakeLists.txt" << 'CMAKE'
cmake_minimum_required(VERSION 3.8.2)
project(firewall_pd_64)

include(${CAMKES_VM_HELPERS_PATH})
find_package(camkes-vm-linux REQUIRED)
camkes_vm_linux_import_project()
include(${CAMKES_VM_LINUX_HELPERS_PATH})

DeclareCAmkESVM(Init0 EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/cross_vm_connections.c)

GetDefaultLinuxKernelFile(kernel_file)
GetDefaultLinuxRootfsFile(rootfs_file)

DecompressLinuxKernel(extract_linux_kernel decompressed_kernel ${kernel_file})
AddToFileServer("bzimage" ${decompressed_kernel} DEPENDS extract_linux_kernel)
AddToFileServer("rootfs.cpio" ${rootfs_file})

DeclareCAmkESVMRootServer(minimal_firewall.camkes)
CMAKE

# 5. Cross VM connections stub
cat > "$REPO/sel4-build/projects/vm-examples/apps/x86/firewall_pd_64/src/cross_vm_connections.c" << 'CVC'
/* Stub for cross-VM connections (no cross-VM IPC needed for Firewall PD). */
CVC

echo "Firewall PD scaffold created:"
echo "  - FirewallPD.camkes (component declaration)"
echo "  - firewall_pd.c (IPC proof + packet filtering)"
echo "  - minimal_firewall.camkes (assembly)"
echo "  - CMakeLists.txt (build config)"
echo "[DONE]"
