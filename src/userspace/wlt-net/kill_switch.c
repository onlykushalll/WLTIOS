/**
 * @file kill_switch.c
 * @brief Kill switch — block ALL traffic when Tor dies
 * @module wlt-net/kill_switch
 * @purpose Maintain two firewall rule sets:
 *          (1) Tor-only: loopback + Tor SOCKS/Control/DNS only.
 *          (2) Block-all: loopback only, no egress at all.
 *          Switching between them happens in <100ms (spec budget).
 * @dependencies: net.h
 * @tested_by: tests/userspace/kill_switch_test.c
 * @spec: docs/specs/wlt-net.md
 *
 * Defense-in-depth: the seL4 Firewall PD is the authoritative enforcer.
 * These rules are a second layer inside the main Linux guest so that
 * even a process that bypasses the Firewall PD queue cannot leak.
 */

#include "net.h"
#include "tor_proxy.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#define WLT_KS_NFT_TABLE "wlt_net"
#define WLT_KS_NFT_CHAIN_IN  "wlt_input"
#define WLT_KS_NFT_CHAIN_OUT "wlt_output"
#define WLT_KS_NFT_CHAIN_FWD "wlt_forward"

/* ─── Backend selection ──────────────────────────────── */

bool
wlt_kill_switch_nftables_available(void)
{
    struct stat st;
    if (stat(WLT_NET_NFT_CMD, &st) != 0)
        return false;
    if (!S_ISREG(st.st_mode))
        return false;
    if ((st.st_mode & 0111) == 0)
        return false;

    /* Probe: `nft list tables` exits 0 only if nft is usable. */
    char *argv[] = { (char *)"nft", (char *)"list", (char *)"tables", NULL };
    int exit_code = -1;
    if (wlt_net_run_cmd(WLT_NET_NFT_CMD, argv, &exit_code) != WLT_NET_OK)
        return false;
    return exit_code == 0;
}

/* ─── nftables helpers ───────────────────────────────── */

static wlt_net_error_t
run_nft(const char *spec)
{
    char *argv[] = { (char *)"nft", (char *)spec, NULL };
    int exit_code = -1;
    wlt_net_error_t err = wlt_net_run_cmd(WLT_NET_NFT_CMD, argv, &exit_code);
    if (err != WLT_NET_OK)
        return err;
    if (exit_code != 0)
        return WLT_NET_ERR_KILL_SWITCH;
    return WLT_NET_OK;
}

static wlt_net_error_t
nft_reset_table(void)
{
    /* delete + recreate to guarantee a clean slate. delete may fail
     * on first run — that's fine. */
    char spec[128];
    snprintf(spec, sizeof(spec), "delete table inet %s", WLT_KS_NFT_TABLE);
    (void)run_nft(spec);

    snprintf(spec, sizeof(spec), "add table inet %s", WLT_KS_NFT_TABLE);
    wlt_net_error_t err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    /* Three chains, all default DROP. Order matters: build them before
     * adding rules so rule-add never races with hook activation. */
    snprintf(spec, sizeof(spec),
             "add chain inet %s %s { type filter hook input priority 0; policy drop; }",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_IN);
    err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    snprintf(spec, sizeof(spec),
             "add chain inet %s %s { type filter hook output priority 0; policy drop; }",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_OUT);
    err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    snprintf(spec, sizeof(spec),
             "add chain inet %s %s { type filter hook forward priority 0; policy drop; }",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_FWD);
    return run_nft(spec);
}

static wlt_net_error_t
nft_allow_loopback(void)
{
    char spec[256];
    snprintf(spec, sizeof(spec),
             "add rule inet %s %s iifname \"lo\" accept",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_IN);
    wlt_net_error_t err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    snprintf(spec, sizeof(spec),
             "add rule inet %s %s oifname \"lo\" accept",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_OUT);
    return run_nft(spec);
}

static wlt_net_error_t
nft_allow_tor(uint16_t socks_port, uint16_t dns_port)
{
    char spec[256];

    /* Tor SOCKS (TCP 9050) on loopback. */
    snprintf(spec, sizeof(spec),
             "add rule inet %s %s ip daddr 127.0.0.1 tcp dport %u accept",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_OUT, socks_port);
    wlt_net_error_t err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    /* Tor control (TCP 9051) on loopback. */
    snprintf(spec, sizeof(spec),
             "add rule inet %s %s ip daddr 127.0.0.1 tcp dport %u accept",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_OUT,
             WLT_TOR_CONTROL_PORT);
    err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    /* Tor DNS (UDP 5353) on loopback — no system DNS, ever. */
    snprintf(spec, sizeof(spec),
             "add rule inet %s %s ip daddr 127.0.0.1 udp dport %u accept",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_OUT, dns_port);
    err = run_nft(spec);
    if (err != WLT_NET_OK)
        return err;

    /* Allow replies from loopback services back to local sockets.
     * iifname-restricted (not just saddr) as belt-and-suspenders against
     * a spoofed-source packet arriving on a non-lo interface, even though
     * the kernel's own martian-packet check should already drop
     * 127.0.0.0/8-sourced packets seen on external interfaces. This file's
     * whole purpose is defense-in-depth, so it shouldn't lean on an
     * unstated kernel behavior for the one rule that skipped it. */
    snprintf(spec, sizeof(spec),
             "add rule inet %s %s iifname \"lo\" ip saddr 127.0.0.1 accept",
             WLT_KS_NFT_TABLE, WLT_KS_NFT_CHAIN_IN);
    return run_nft(spec);
}

/* ─── iptables helpers ───────────────────────────────── */

static wlt_net_error_t
run_iptables(char *const argv[])
{
    int exit_code = -1;
    wlt_net_error_t err = wlt_net_run_cmd(WLT_NET_IPTABLES_CMD,
                                            argv, &exit_code);
    if (err != WLT_NET_OK)
        return err;
    if (exit_code != 0)
        return WLT_NET_ERR_KILL_SWITCH;
    return WLT_NET_OK;
}

static wlt_net_error_t
iptables_reset_chains(void)
{
    char *flush[] = { (char *)"iptables", (char *)"-F", NULL };
    char *flush_nat[] = { (char *)"iptables",
                          (char *)"-t", (char *)"nat",
                          (char *)"-F", NULL };
    char *p_in[]  = { (char *)"iptables", (char *)"-P",
                      (char *)"INPUT",   (char *)"DROP", NULL };
    char *p_out[] = { (char *)"iptables", (char *)"-P",
                      (char *)"OUTPUT",  (char *)"DROP", NULL };
    char *p_fwd[] = { (char *)"iptables", (char *)"-P",
                      (char *)"FORWARD", (char *)"DROP", NULL };

    (void)run_iptables(flush);
    (void)run_iptables(flush_nat);
    wlt_net_error_t err = run_iptables(p_in);
    if (err != WLT_NET_OK)
        return err;
    err = run_iptables(p_out);
    if (err != WLT_NET_OK)
        return err;
    err = run_iptables(p_fwd);
    return err;
}

static wlt_net_error_t
iptables_allow_loopback(void)
{
    char *in_lo[]  = { (char *)"iptables", (char *)"-A",
                       (char *)"INPUT",  (char *)"-i",
                       (char *)"lo",     (char *)"-j",
                       (char *)"ACCEPT", NULL };
    char *out_lo[] = { (char *)"iptables", (char *)"-A",
                       (char *)"OUTPUT", (char *)"-o",
                       (char *)"lo",     (char *)"-j",
                       (char *)"ACCEPT", NULL };
    wlt_net_error_t err = run_iptables(in_lo);
    if (err != WLT_NET_OK)
        return err;
    return run_iptables(out_lo);
}

static wlt_net_error_t
iptables_allow_tor(uint16_t socks_port, uint16_t dns_port)
{
    char socks_arg[16];
    char ctrl_arg[16];
    char dns_arg[16];
    snprintf(socks_arg, sizeof(socks_arg), "%u", socks_port);
    snprintf(ctrl_arg, sizeof(ctrl_arg), "%u", WLT_TOR_CONTROL_PORT);
    snprintf(dns_arg,   sizeof(dns_arg),   "%u", dns_port);

    char *a1[] = { (char *)"iptables", (char *)"-A", (char *)"OUTPUT",
                   (char *)"-d", (char *)"127.0.0.1",
                   (char *)"-p", (char *)"tcp",
                   (char *)"--dport", socks_arg,
                   (char *)"-j", (char *)"ACCEPT", NULL };
    char *a2[] = { (char *)"iptables", (char *)"-A", (char *)"OUTPUT",
                   (char *)"-d", (char *)"127.0.0.1",
                   (char *)"-p", (char *)"tcp",
                   (char *)"--dport", ctrl_arg,
                   (char *)"-j", (char *)"ACCEPT", NULL };
    char *a3[] = { (char *)"iptables", (char *)"-A", (char *)"OUTPUT",
                   (char *)"-d", (char *)"127.0.0.1",
                   (char *)"-p", (char *)"udp",
                   (char *)"--dport", dns_arg,
                   (char *)"-j", (char *)"ACCEPT", NULL };
    char *a4[] = { (char *)"iptables", (char *)"-A", (char *)"INPUT",
                   (char *)"-i", (char *)"lo",
                   (char *)"-s", (char *)"127.0.0.1",
                   (char *)"-j", (char *)"ACCEPT", NULL };

    wlt_net_error_t err = run_iptables(a1);
    if (err != WLT_NET_OK)
        return err;
    err = run_iptables(a2);
    if (err != WLT_NET_OK)
        return err;
    err = run_iptables(a3);
    if (err != WLT_NET_OK)
        return err;
    return run_iptables(a4);
}

/* ─── Public API ─────────────────────────────────────── */

wlt_net_error_t
wlt_kill_switch_install(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    /* Install fail-closed rules immediately — Tor-only by default. */
    if (state->tor_state == WLT_NET_TOR_READY)
        return wlt_kill_switch_apply_tor_only(state);
    return wlt_kill_switch_apply_block(state);
}

wlt_net_error_t
wlt_kill_switch_apply_block(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    WLT_NET_LOG_WRN("KILL SWITCH: blocking ALL traffic");

    wlt_net_error_t err;
    if (state->use_nftables)
    {
        err = nft_reset_table();
        if (err != WLT_NET_OK)
            return err;
        err = nft_allow_loopback();
    }
    else
    {
        err = iptables_reset_chains();
        if (err != WLT_NET_OK)
            return err;
        err = iptables_allow_loopback();
    }

    if (err != WLT_NET_OK)
    {
        WLT_NET_LOG_FTL("KILL SWITCH failed to apply: %s",
                         wlt_net_error_str(err));
        return err;
    }
    state->kill_switch_active = true;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_kill_switch_apply_tor_only(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    WLT_NET_LOG_INF("Firewall: Tor-only rules");

    wlt_net_error_t err;
    if (state->use_nftables)
    {
        err = nft_reset_table();
        if (err != WLT_NET_OK)
            return err;
        err = nft_allow_loopback();
        if (err != WLT_NET_OK)
            return err;
        err = nft_allow_tor(state->tor_socks_port, state->tor_dns_port);
    }
    else
    {
        err = iptables_reset_chains();
        if (err != WLT_NET_OK)
            return err;
        err = iptables_allow_loopback();
        if (err != WLT_NET_OK)
            return err;
        err = iptables_allow_tor(state->tor_socks_port, state->tor_dns_port);
    }

    if (err != WLT_NET_OK)
    {
        WLT_NET_LOG_FTL("Tor-only rule install failed: %s — "
                         "falling back to block-all",
                         wlt_net_error_str(err));
        /* Fail-closed: if we can't install Tor-only, block everything. */
        return wlt_kill_switch_apply_block(state);
    }

    state->kill_switch_active = false;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_kill_switch_cleanup(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    if (state->use_nftables)
    {
        char spec[128];
        snprintf(spec, sizeof(spec),
                 "delete table inet %s", WLT_KS_NFT_TABLE);
        (void)run_nft(spec);
    }
    else
    {
        char *flush[] = { (char *)"iptables", (char *)"-F", NULL };
        char *p_in[]  = { (char *)"iptables", (char *)"-P",
                          (char *)"INPUT",   (char *)"ACCEPT", NULL };
        char *p_out[] = { (char *)"iptables", (char *)"-P",
                          (char *)"OUTPUT",  (char *)"ACCEPT", NULL };
        char *p_fwd[] = { (char *)"iptables", (char *)"-P",
                          (char *)"FORWARD", (char *)"ACCEPT", NULL };
        (void)run_iptables(flush);
        (void)run_iptables(p_in);
        (void)run_iptables(p_out);
        (void)run_iptables(p_fwd);
    }
    state->kill_switch_active = false;
    return WLT_NET_OK;
}
