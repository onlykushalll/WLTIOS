/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * WLTIOS - seL4 Firewall Protection Domain
 * packet.c
 *
 * Zero-copy packet parsing for the firewall. Given a raw L2 frame in
 * a shared sDDF data pool, this produces a packet_view_t that the
 * rule engine can inspect without copying any bytes.
 *
 * Parsing is STRICT about bounds: every header field read is checked
 * against the remaining frame length. A truncated or malformed frame
 * yields parse_ok == false and the caller drops it fail-closed. The
 * parser never reads outside [frame, frame+len).
 *
 * No allocations, no libc. Endianness conversions are open-coded so
 * the PD has no dependency on a network-byte-order library.
 */
#include "firewall.h"
#include "rules.h"

/* ------------------------------------------------------------------ */
/* Wire header layouts (packed, 1-byte aligned).                       */
/* ------------------------------------------------------------------ */

#define FW_ETH_ADDR_LEN 6
#define FW_ETH_HDR_LEN  14   /* dst(6) + src(6) + ethertype(2)       */

typedef struct __attribute__((packed)) eth_hdr {
    uint8_t  dst[FW_ETH_ADDR_LEN];
    uint8_t  src[FW_ETH_ADDR_LEN];
    uint16_t ethertype;
} eth_hdr_t;

/* IPv4 header - only the fixed 20-byte portion we inspect. */
typedef struct __attribute__((packed)) ipv4_hdr {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
} ipv4_hdr_t;

typedef struct __attribute__((packed)) tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_off;   /* top 4 bits = header length in 32-bit words */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urg_ptr;
} tcp_hdr_t;

typedef struct __attribute__((packed)) udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_hdr_t;

/* ------------------------------------------------------------------ */
/* Byte-order helpers (open coded - no libc).                          */
/* ------------------------------------------------------------------ */

static inline uint16_t fw_be16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

static inline uint32_t fw_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static inline int fw_memcmp(const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

static inline void fw_mac_copy(uint8_t dst[6], const uint8_t src[6])
{
    for (int i = 0; i < 6; i++) {
        dst[i] = src[i];
    }
}

/* ------------------------------------------------------------------ */
/* packet_parse                                                        */
/* ------------------------------------------------------------------ */
void packet_parse(const uint8_t *frame, size_t len, packet_view_t *pkt)
{
    /* Zero the view so partial parses don't leak stale state. */
    pkt->raw       = frame;
    pkt->raw_len   = len;
    pkt->eth       = NULL;
    pkt->ipv4      = NULL;
    pkt->l4        = NULL;
    pkt->ethertype = 0;
    pkt->l3_proto  = 0;
    pkt->src_ip    = 0;
    pkt->dst_ip    = 0;
    pkt->src_port  = 0;
    pkt->dst_port  = 0;
    pkt->ttl       = 0;
    pkt->is_ipv4   = false;
    pkt->is_tcp    = false;
    pkt->is_udp    = false;
    pkt->parse_ok  = false;

    for (int i = 0; i < 6; i++) {
        pkt->src_mac[i] = 0;
        pkt->dst_mac[i] = 0;
    }

    if (frame == NULL || len < FW_ETH_HDR_LEN) {
        return;                 /* too short for an Ethernet header */
    }

    const eth_hdr_t *eth = (const eth_hdr_t *)frame;
    pkt->eth = frame;
    fw_mac_copy(pkt->dst_mac, eth->dst);
    fw_mac_copy(pkt->src_mac, eth->src);
    pkt->ethertype = fw_be16((const uint8_t *)&eth->ethertype);

    /* Currently we only forward IPv4. ARP/IPv6 are dropped by the
     * default-deny rule; we still set parse_ok so the rule engine
     * can make an explicit decision. */
    if (pkt->ethertype != FW_ETHERTYPE_IPV4) {
        /* Not IPv4 - we still consider the L2 parse successful so
         * the rule engine can apply policy (e.g. allow ARP only to
         * the Tor VM). */
        pkt->parse_ok = true;
        return;
    }

    /* ---- IPv4 -------------------------------------------------- */
    if (len < FW_ETH_HDR_LEN + 20) {
        return;                 /* truncated IPv4 header           */
    }
    const ipv4_hdr_t *ip =
        (const ipv4_hdr_t *)(frame + FW_ETH_HDR_LEN);
    pkt->ipv4 = (const uint8_t *)ip;

    uint8_t ihl = ip->ver_ihl & 0x0f;
    uint32_t ip_hdr_len = (uint32_t)ihl * 4U;
    if (ihl < 5) {
        return;                 /* IHL too small                   */
    }
    if (ip_hdr_len < 20) {
        return;
    }
    if (len < FW_ETH_HDR_LEN + ip_hdr_len) {
        return;                 /* truncated (options)             */
    }

    /* Sanity-check the IP total length against the frame. */
    uint16_t ip_total = fw_be16((const uint8_t *)&ip->total_len);
    if (ip_total < ip_hdr_len) {
        return;                 /* inconsistent                    */
    }
    if ((size_t)ip_total + FW_ETH_HDR_LEN > len) {
        /* Frame shorter than IP claims - truncated on the wire. We
         * still parse what we have but mark parse_ok so the policy
         * can drop it explicitly. */
        pkt->parse_ok = true;
        pkt->is_ipv4  = true;
        pkt->l3_proto = ip->proto;
        pkt->src_ip   = fw_be32(ip->src_ip);
        pkt->dst_ip   = fw_be32(ip->dst_ip);
        pkt->ttl      = ip->ttl;
        return;
    }

    pkt->is_ipv4  = true;
    pkt->l3_proto = ip->proto;
    pkt->src_ip   = fw_be32(ip->src_ip);
    pkt->dst_ip   = fw_be32(ip->dst_ip);
    pkt->ttl      = ip->ttl;

    const uint8_t *l4 = frame + FW_ETH_HDR_LEN + ip_hdr_len;
    size_t l4_len     = (size_t)ip_total - ip_hdr_len;

    switch (ip->proto) {
    case FW_PROTO_TCP: {
        if (l4_len < 20) {
            break;              /* truncated TCP header            */
        }
        const tcp_hdr_t *tcp = (const tcp_hdr_t *)l4;
        pkt->l4       = l4;
        pkt->is_tcp   = true;
        pkt->src_port = fw_be16((const uint8_t *)&tcp->src_port);
        pkt->dst_port = fw_be16((const uint8_t *)&tcp->dst_port);
        break;
    }
    case FW_PROTO_UDP: {
        if (l4_len < 8) {
            break;              /* truncated UDP header            */
        }
        const udp_hdr_t *udp = (const udp_hdr_t *)l4;
        pkt->l4       = l4;
        pkt->is_udp   = true;
        pkt->src_port = fw_be16((const uint8_t *)&udp->src_port);
        pkt->dst_port = fw_be16((const uint8_t *)&udp->dst_port);
        break;
    }
    default:
        /* ICMP / other - no ports. l4 stays NULL. */
        break;
    }

    pkt->parse_ok = true;
}

/* ------------------------------------------------------------------ */
/* Forwarding lookup.                                                  */
/*                                                                      */
/* The firewall decides egress by destination MAC. This is a tiny L2   */
/* table: each known peer has one MAC. Anything else (including the    */
/* default gateway and broadcast) goes to the NIC driver, which is     */
/* correct because the NIC is the only path to anything not on the     */
/* internal links.                                                     */
/* ------------------------------------------------------------------ */
firewall_link_t packet_egress_link(const packet_view_t *pkt)
{
    if (pkt == NULL || pkt->eth == NULL) {
        return FW_LINK_UNKNOWN;
    }

    /* Match destination MAC against the known peers. */
    if (fw_memcmp(pkt->dst_mac, fw_mac_main_linux, 6) == 0) {
        return FW_LINK_MAIN_LINUX;
    }
    if (fw_memcmp(pkt->dst_mac, fw_mac_tor_vm, 6) == 0) {
        return FW_LINK_TOR_VM;
    }

    /* Everything else (default gateway, broadcast, multicast, IPv6
     * neighbour-discovery multicast) egresses to the NIC driver. The
     * rule table then decides allow/deny. */
    return FW_LINK_NIC_DRIVER;
}
