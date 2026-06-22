/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * WLTIOS - seL4 Firewall Protection Domain
 * include/rules.h
 *
 * Firewall rule types and the matching engine interface.
 *
 * Rules are COMPILED INTO the PD at build time (see rules.c). Linux
 * cannot add, remove or modify rules at runtime - the rule table
 * lives in read-only memory owned by the firewall PD. This is a core
 * security property: even a fully-compromised Linux guest cannot
 * weaken the Tor-only policy.
 *
 * Rule semantics
 * --------------
 *  - Rules are evaluated in table order; the FIRST match wins.
 *  - A field set to FW_MATCH_ANY matches anything.
 *  - If no rule matches, the policy is DEFAULT DENY (fail-closed).
 *  - The kill switch (see kill_switch.c) is checked BEFORE the rule
 *    table and overrides everything: when armed, every packet is
 *    denied with reason FW_REASON_KILL_SWITCH.
 *
 * The compiled table encodes the policy from the spec:
 *
 *    src              dst              proto  dport   action  comment
 *    ------------------------------------------------------------------
 *    MAIN_LINUX       NIC              any    any     DENY    direct net
 *    MAIN_LINUX       TOR_VM           any    any     ALLOW   route via Tor
 *    TOR_VM           MAIN_LINUX       any    any     ALLOW   responses
 *    TOR_VM           NIC              any    any     ALLOW   Tor egress
 *    any              any              UDP    53      DENY*   DNS leak guard
 *    any              any              any    any     DENY    default deny
 *
 *    * unless dst_ip == FW_TOR_DNS_IP (the in-PD Tor resolver).
 */
#ifndef WLTIOS_RULES_H
#define WLTIOS_RULES_H

#include <stdint.h>
#include <stdbool.h>
#include "firewall.h"

/* ==================================================================
 * Match wildcards.
 * ================================================================== */
#define FW_MATCH_ANY_LINK   ((uint8_t)FW_LINK_UNKNOWN) /* 0xff        */
#define FW_MATCH_ANY_PROTO  (0)        /* proto field wildcard         */
#define FW_MATCH_ANY_PORT   (0)        /* port field wildcard          */
#define FW_MATCH_ANY_IP     (0U)       /* 0.0.0.0 = any                */

/* Helper: build an IPv4 host-order address from octets. */
#define FW_IPV4(a,b,c,d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(c) << 8)  |  (uint32_t)(d))

/* ==================================================================
 * Build-time network configuration.
 *
 * These constants describe the on-link addresses the firewall uses to
 * make forwarding and policy decisions. They are #defines so the
 * whole policy is visible at a glance and resolved at compile time.
 * ================================================================== */

/* Ethertype values we care about. */
#define FW_ETHERTYPE_IPV4   0x0800
#define FW_ETHERTYPE_ARP    0x0806
#define FW_ETHERTYPE_IPV6   0x86DD

/* IP port of the in-PD Tor DNS resolver (DNS to anything else is a
 * leak and is blocked). */
#define FW_TOR_DNS_PORT     5353
#define FW_STANDARD_DNS     53

/* IPv4 (host order) of the Tor VM's resolver. DNS packets addressed
 * here are allowed; DNS to any other address is dropped. */
#define FW_TOR_DNS_IP       FW_IPV4(10,99,0,53)

/* Link addresses used by the forwarding lookup. The firewall learns
 * egress by comparing the destination Ethernet address against these.
 * (Broadcast / unknown -> NIC so the driver can drop or flood.) */
extern const uint8_t fw_mac_main_linux[6];
extern const uint8_t fw_mac_tor_vm[6];
extern const uint8_t fw_mac_nic_gw[6];     /* default-gateway MAC      */

/* ==================================================================
 * Rule descriptor.
 *
 * Each rule fully describes one policy decision. Fields set to a
 * FW_MATCH_* wildcard match anything; all non-wildcard fields must
 * match for the rule to fire.
 * ================================================================== */
typedef struct firewall_rule {
    uint8_t      src_link;     /* FW_LINK_* or FW_MATCH_ANY_LINK       */
    uint8_t      dst_link;     /* FW_LINK_* or FW_MATCH_ANY_LINK       */
    uint8_t      proto;        /* FW_PROTO_* or FW_MATCH_ANY_PROTO     */
    uint16_t     src_port;     /* host order, or FW_MATCH_ANY_PORT     */
    uint16_t     dst_port;     /* host order, or FW_MATCH_ANY_PORT     */
    uint32_t     src_ip;       /* host order, or FW_MATCH_ANY_IP       */
    uint32_t     dst_ip;       /* host order, or FW_MATCH_ANY_IP       */
    uint8_t      action;       /* firewall_action_t                    */
    uint8_t      reason;       /* firewall_reason_t tagged on match    */
    const char  *name;         /* human-readable rule id (static)      */
} firewall_rule_t;

/* The compiled rule table. Defined in rules.c, placed in .rodata. */
extern const firewall_rule_t fw_rules[];
extern const unsigned int   fw_rules_count;

/* ==================================================================
 * Matching primitives (used by rules.c; exposed for unit tests).
 * ================================================================== */

/* True if @rule matches the given packet + path. A rule matches when
 * every non-wildcard field is satisfied. */
bool rule_matches(const firewall_rule_t *rule,
                  const packet_view_t *pkt,
                  firewall_link_t src,
                  firewall_link_t dst);

/* Pretty-print a single rule to the debug console (no-op unless
 * FW_DEBUG). */
void rule_dump(const firewall_rule_t *rule);

#endif /* WLTIOS_RULES_H */
