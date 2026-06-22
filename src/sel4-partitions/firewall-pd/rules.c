/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * WLTIOS - seL4 Firewall Protection Domain
 * rules.c
 *
 * The compiled-in rule table and the matching engine.
 *
 * Security note: this table is the entire network policy. It lives in
 * .rodata and Linux has no way to reach it (seL4 enforces the PD
 * boundary). Adding or changing a rule requires rebuilding the PD,
 * which only WLT-Init can do at boot. This is what makes the Tor-only
 * guarantee a formal property rather than a configuration.
 *
 * Evaluation order:
 *   1. Kill switch armed  -> DENY  (reason FW_REASON_KILL_SWITCH)
 *   2. First matching rule -> its action
 *   3. No match           -> DENY  (reason FW_REASON_DEFAULT_DENY)
 *
 * DNS leak guard: a rule drops any UDP/53 packet whose destination is
 * not the in-PD Tor resolver (FW_TOR_DNS_IP). This prevents a
 * compromised guest from bypassing Tor by querying an external DNS
 * server directly.
 */
#include "firewall.h"
#include "rules.h"

/* ------------------------------------------------------------------ */
/* Link MAC addresses (build-time configuration).                      */
/* These are the addresses the firewall uses to identify the source   */
/* and destination links. They must match the guest VM configuration. */
/* ------------------------------------------------------------------ */
const uint8_t fw_mac_main_linux[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
const uint8_t fw_mac_tor_vm[6]     = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
const uint8_t fw_mac_nic_gw[6]     = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x03 };

/* ------------------------------------------------------------------ */
/* The policy table.                                                   */
/*                                                                      */
/* Order matters: the first match wins. The DNS-leak guard sits above  */
/* the generic allow rules so it overrides them; the final default-    */
/* deny rule catches anything that slipped through.                    */
/* ------------------------------------------------------------------ */
#define DENY  FW_ACTION_DENY
#define ALLOW FW_ACTION_ALLOW

const firewall_rule_t fw_rules[] = {
    /* --- DNS leak prevention ------------------------------------- */
    /* DNS (UDP/53) to anything except the Tor resolver is blocked. */
    {
        .src_link = FW_MATCH_ANY_LINK,
        .dst_link = FW_MATCH_ANY_LINK,
        .proto    = FW_PROTO_UDP,
        .src_port = FW_MATCH_ANY_PORT,
        .dst_port = FW_STANDARD_DNS,
        .src_ip   = FW_MATCH_ANY_IP,
        .dst_ip   = FW_MATCH_ANY_IP,   /* handled specially below    */
        .action   = DENY,
        .reason   = FW_REASON_DNS_LEAK,
        .name     = "dns-leak-guard",
    },

    /* --- Tor-only egress policy --------------------------------- */
    /* Main Linux may NOT reach the NIC directly; it must go through
     * the Tor VM. This is the central Tor-only rule. */
    {
        .src_link = FW_LINK_MAIN_LINUX,
        .dst_link = FW_LINK_NIC_DRIVER,
        .proto    = FW_PROTO_ANY,
        .src_port = FW_MATCH_ANY_PORT,
        .dst_port = FW_MATCH_ANY_PORT,
        .src_ip   = FW_MATCH_ANY_IP,
        .dst_ip   = FW_MATCH_ANY_IP,
        .action   = DENY,
        .reason   = FW_REASON_RULE_MATCH,
        .name     = "main-no-direct-egress",
    },

    /* Main Linux may talk to the Tor VM (this is how it routes
     * through Tor). */
    {
        .src_link = FW_LINK_MAIN_LINUX,
        .dst_link = FW_LINK_TOR_VM,
        .proto    = FW_PROTO_ANY,
        .src_port = FW_MATCH_ANY_PORT,
        .dst_port = FW_MATCH_ANY_PORT,
        .src_ip   = FW_MATCH_ANY_IP,
        .dst_ip   = FW_MATCH_ANY_IP,
        .action   = ALLOW,
        .reason   = FW_REASON_RULE_MATCH,
        .name     = "main-to-tor",
    },

    /* Tor VM may return traffic to Main Linux (Tor responses). */
    {
        .src_link = FW_LINK_TOR_VM,
        .dst_link = FW_LINK_MAIN_LINUX,
        .proto    = FW_PROTO_ANY,
        .src_port = FW_MATCH_ANY_PORT,
        .dst_port = FW_MATCH_ANY_PORT,
        .src_ip   = FW_MATCH_ANY_IP,
        .dst_ip   = FW_MATCH_ANY_IP,
        .action   = ALLOW,
        .reason   = FW_REASON_RULE_MATCH,
        .name     = "tor-to-main",
    },

    /* Tor VM may egress to the NIC (Tor's own encrypted traffic). */
    {
        .src_link = FW_LINK_TOR_VM,
        .dst_link = FW_LINK_NIC_DRIVER,
        .proto    = FW_PROTO_ANY,
        .src_port = FW_MATCH_ANY_PORT,
        .dst_port = FW_MATCH_ANY_PORT,
        .src_ip   = FW_MATCH_ANY_IP,
        .dst_ip   = FW_MATCH_ANY_IP,
        .action   = ALLOW,
        .reason   = FW_REASON_RULE_MATCH,
        .name     = "tor-egress",
    },

    /* --- Default deny ------------------------------------------- */
    /* Anything not explicitly allowed above is dropped. This is the
     * fail-closed catch-all. */
    {
        .src_link = FW_MATCH_ANY_LINK,
        .dst_link = FW_MATCH_ANY_LINK,
        .proto    = FW_PROTO_ANY,
        .src_port = FW_MATCH_ANY_PORT,
        .dst_port = FW_MATCH_ANY_PORT,
        .src_ip   = FW_MATCH_ANY_IP,
        .dst_ip   = FW_MATCH_ANY_IP,
        .action   = DENY,
        .reason   = FW_REASON_DEFAULT_DENY,
        .name     = "default-deny",
    },
};

const unsigned int fw_rules_count =
    sizeof(fw_rules) / sizeof(fw_rules[0]);

unsigned int rule_count(void)
{
    return fw_rules_count;
}

/* ------------------------------------------------------------------ */
/* Matching.                                                           */
/* ------------------------------------------------------------------ */
bool rule_matches(const firewall_rule_t *rule,
                  const packet_view_t *pkt,
                  firewall_link_t src,
                  firewall_link_t dst)
{
    if (rule == NULL || pkt == NULL) {
        return false;
    }

    /* Link selectors. */
    if (rule->src_link != FW_MATCH_ANY_LINK && rule->src_link != (uint8_t)src) {
        return false;
    }
    if (rule->dst_link != FW_MATCH_ANY_LINK && rule->dst_link != (uint8_t)dst) {
        return false;
    }

    /* L3 protocol. */
    if (rule->proto != FW_MATCH_ANY_PROTO && rule->proto != pkt->l3_proto) {
        return false;
    }

    /* Ports only meaningful for TCP/UDP. If the rule specifies a port
     * but the packet has no ports, it cannot match. */
    if (rule->src_port != FW_MATCH_ANY_PORT) {
        if (!(pkt->is_tcp || pkt->is_udp)) {
            return false;
        }
        if (rule->src_port != pkt->src_port) {
            return false;
        }
    }
    if (rule->dst_port != FW_MATCH_ANY_PORT) {
        if (!(pkt->is_tcp || pkt->is_udp)) {
            return false;
        }
        if (rule->dst_port != pkt->dst_port) {
            return false;
        }
    }

    /* IP addresses (host order). 0 == wildcard. */
    if (rule->src_ip != FW_MATCH_ANY_IP && rule->src_ip != pkt->src_ip) {
        return false;
    }
    if (rule->dst_ip != FW_MATCH_ANY_IP && rule->dst_ip != pkt->dst_ip) {
        return false;
    }

    /* ---- DNS leak guard special-case ---------------------------- *
     * The dns-leak-guard rule is written with dst_ip == ANY so that
     * it matches all UDP/53 traffic; here we RE-ALLOW if the
     * destination is the Tor resolver. This keeps the table compact
     * (one DNS rule instead of two) and makes the exception obvious. */
    if (rule->reason == FW_REASON_DNS_LEAK) {
        if (pkt->dst_ip == FW_TOR_DNS_IP) {
            /* DNS to the Tor resolver is fine - don't drop here. */
            return false;
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* Top-level evaluation.                                               */
/* ------------------------------------------------------------------ */
firewall_action_t rule_evaluate(const packet_view_t *pkt,
                                firewall_link_t src,
                                firewall_link_t dst,
                                int *rule_idx,
                                firewall_reason_t *reason)
{
    if (rule_idx != NULL) {
        *rule_idx = -1;
    }

    /* 1. Kill switch overrides everything. */
    if (kill_switch_is_active()) {
        if (reason != NULL) {
            *reason = FW_REASON_KILL_SWITCH;
        }
        return FW_ACTION_DENY;
    }

    /* Malformed packet -> fail-closed. */
    if (pkt == NULL || !pkt->parse_ok) {
        if (reason != NULL) {
            *reason = FW_REASON_PARSE_ERROR;
        }
        return FW_ACTION_DENY;
    }

    /* Forwarding loop guard: never send a packet back out the link it
     * arrived on. */
    if (src == dst) {
        if (reason != NULL) {
            *reason = FW_REASON_FORWARD_LOOP;
        }
        return FW_ACTION_DENY;
    }

    /* 2. Walk the table; first match wins. */
    for (unsigned int i = 0; i < fw_rules_count; i++) {
        if (rule_matches(&fw_rules[i], pkt, src, dst)) {
            if (rule_idx != NULL) {
                *rule_idx = (int)i;
            }
            if (reason != NULL) {
                *reason = (firewall_reason_t)fw_rules[i].reason;
            }
            return (firewall_action_t)fw_rules[i].action;
        }
    }

    /* 3. No match -> default deny. */
    if (reason != NULL) {
        *reason = FW_REASON_DEFAULT_DENY;
    }
    return FW_ACTION_DENY;
}

/* ------------------------------------------------------------------ */
/* Diagnostics.                                                         */
/* ------------------------------------------------------------------ */
void rule_dump(const firewall_rule_t *rule)
{
    (void)rule;
#ifdef FW_DEBUG
    if (rule == NULL) {
        fw_log("rule=<null>\n");
        return;
    }
    fw_log("rule[%s] src=%u dst=%u proto=%u sport=%u dport=%u action=%c\n",
           rule->name ? rule->name : "?",
           rule->src_link, rule->dst_link, rule->proto,
           rule->src_port, rule->dst_port,
           rule->action == FW_ACTION_ALLOW ? 'A' : 'D');
#endif
}
