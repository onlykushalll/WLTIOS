/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * WLTIOS - seL4 Firewall Protection Domain
 * include/firewall.h
 *
 * Public interface for the Firewall PD. This PD sits in the network
 * data path between the Linux guests (Main Linux + Tor VM) and the
 * NIC driver PD. It enforces the Tor-only forwarding policy and the
 * network kill switch.
 *
 * Topology (see docs/specs/firewall-pd.md):
 *
 *   Main Linux Guest ---+
 *                        +--> Firewall PD --> NIC Driver PD --> Hardware
 *   Tor VM -------------+          ^
 *                                 |
 *                          Drops non-Tor traffic.
 *                          Kill switch: drops ALL traffic.
 *
 * The Firewall PD has NO filesystem, NO libc and NO sockets. All
 * packet I/O uses seL4 Microkit notifications + sDDF shared-memory
 * ring buffers. Rules are compiled in at build time and cannot be
 * modified by Linux at runtime.
 *
 * Operating principle: FAIL-CLOSED. Any unexpected condition results
 * in the packet being dropped. Serious faults (e.g. NIC PD
 * unresponsive) arm the kill switch so that NO traffic passes.
 *
 * Performance budgets (see spec):
 *   - forwarding latency : < 10 us
 *   - throughput         : > 1 Gbps (line rate)
 *   - kill switch trigger : < 100 us
 *   - PD RAM             : < 2 MB
 */
#ifndef WLTIOS_FIREWALL_H
#define WLTIOS_FIREWALL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <microkit.h>

/* ==================================================================
 * Microkit channels (must match the SDF <channel> entries).
 * ================================================================== */
#define FW_CH_NIC_DRIVER   1   /* NIC Driver PD - egress to hardware  */
#define FW_CH_TOR_VM       2   /* Tor VM guest                        */
#define FW_CH_MAIN_LINUX   3   /* Main Linux guest                    */
#define FW_CH_WLT_TOR_MGR  4   /* WLT-Tor-Mgr (kill switch + status)  */

#define FW_PD_ID           2   /* matches SDF protection_domain id=2  */
#define FW_PD_PRIORITY     100 /* matches SDF priority                */

/* ==================================================================
 * Buffer / queue geometry.
 *
 * sDDF rings are power-of-two sized single-producer single-consumer
 * queues of buffer descriptors. The descriptor's @offset indexes into
 * a shared packet data pool that backs the link.
 * ================================================================== */
#define FW_RING_SIZE       512   /* descriptors per ring (power of 2)  */
#define FW_RING_MASK       (FW_RING_SIZE - 1)
#define FW_POOL_SIZE       (1U << 20)  /* 1 MiB data pool per link    */
#define FW_MTU             1518        /* Ethernet MTU incl. header   */
#define FW_MAX_PKT_LEN     FW_MTU

/* Buffer descriptor flags. */
#define FW_BUF_RX          0x0001  /* buffer carries an RX packet    */
#define FW_BUF_TX          0x0002  /* buffer carries a TX packet     */
#define FW_BUF_DROP        0x0004  /* marked for drop (audit trail)  */

/* A descriptor describing one packet buffer inside a shared pool. */
typedef struct net_buff_desc {
    uint64_t offset;   /* byte offset into the link's data pool       */
    uint16_t len;      /* valid payload length (bytes)                */
    uint16_t flags;    /* FW_BUF_*                                    */
    uint32_t cookie;   /* opaque handle (e.g. driver tag) - passthrough */
} net_buff_desc_t;

/*
 * Single-producer / single-consumer ring of buffer descriptors.
 * head = consumer index, tail = producer index. Both advance
 * monotonically and wrap implicitly through the mask. The number of
 * live entries is (tail - head). No locking is required because each
 * ring has exactly one producer and one consumer (the two PDs on
 * either end of a link), and the seL4 mapping is non-cached or
 * cache-coherent across the shared region.
 */
typedef struct net_ring {
    volatile uint32_t head;          /* consumer index               */
    volatile uint32_t tail;          /* producer index               */
    uint32_t size;                   /* capacity (== FW_RING_SIZE)   */
    uint32_t _pad0;                  /* keep 8-byte aligned          */
    net_buff_desc_t buffers[FW_RING_SIZE];
} net_ring_t;

/*
 * A link to one peer PD. Each link carries two directions:
 *
 *   RX  - packets arriving at the firewall from the peer.
 *         rx_used : peer-produced, firewall-consumed.
 *         rx_free : firewall-produced (recycled), peer-consumed.
 *
 *   TX  - packets the firewall sends to the peer.
 *         tx_used : firewall-produced, peer-consumed.
 *         tx_free : peer-produced (recycled), firewall-consumed.
 */
typedef struct net_link {
    net_ring_t *rx_used;             /* peer -> firewall (filled)    */
    net_ring_t *rx_free;             /* firewall -> peer (empty)     */
    net_ring_t *tx_used;             /* firewall -> peer (filled)    */
    net_ring_t *tx_free;             /* peer -> firewall (empty)     */
    uint8_t    *rx_pool;             /* RX data pool base            */
    uint8_t    *tx_pool;             /* TX data pool base            */
    size_t      rx_pool_size;
    size_t      tx_pool_size;
    microkit_channel ch;             /* notification channel to peer */
    const char *name;                /* human-readable link name     */
} net_link_t;

/* Logical links the firewall maintains. */
typedef enum {
    FW_LINK_MAIN_LINUX = 0,
    FW_LINK_TOR_VM     = 1,
    FW_LINK_NIC_DRIVER = 2,
    FW_LINK_COUNT,
    FW_LINK_UNKNOWN    = 0xff
} firewall_link_t;

/* ==================================================================
 * Packet classification.
 * ================================================================== */

/* Which guest produced the packet (determined by ingress link). */
typedef enum {
    FW_SRC_MAIN_LINUX = FW_LINK_MAIN_LINUX,
    FW_SRC_TOR_VM     = FW_LINK_TOR_VM,
    FW_SRC_NIC        = FW_LINK_NIC_DRIVER,
    FW_SRC_UNKNOWN    = FW_LINK_UNKNOWN
} packet_source_t;

/* L3/L4 protocol identifiers we recognise. */
#define FW_PROTO_ANY     0
#define FW_PROTO_ICMP    1
#define FW_PROTO_TCP     6
#define FW_PROTO_UDP     17

/* Parsed packet view. All pointers are into the packet data pool and
 * are only valid for the lifetime of the descriptor's buffer. */
typedef struct packet_view {
    const uint8_t  *raw;            /* start of L2 frame            */
    size_t          raw_len;        /* total frame length           */
    const uint8_t  *eth;            /* Ethernet header              */
    const uint8_t  *ipv4;           /* IPv4 header (NULL if non-IP) */
    const uint8_t  *l4;             /* TCP/UDP header (NULL if n/a) */
    uint16_t        ethertype;
    uint8_t         l3_proto;       /* IP protocol field            */
    uint8_t         src_mac[6];
    uint8_t         dst_mac[6];
    uint32_t        src_ip;         /* host order, 0 if non-IP      */
    uint32_t        dst_ip;         /* host order, 0 if non-IP      */
    uint16_t        src_port;       /* host order, 0 if no ports    */
    uint16_t        dst_port;       /* host order, 0 if no ports    */
    uint8_t         ttl;
    bool            is_ipv4;
    bool            is_tcp;
    bool            is_udp;
    bool            parse_ok;       /* header parse succeeded       */
} packet_view_t;

/* ==================================================================
 * Firewall actions / verdicts.
 * ================================================================== */
typedef enum {
    FW_ACTION_ALLOW = 0,
    FW_ACTION_DENY  = 1
} firewall_action_t;

/* Reason a verdict was reached - used for stats + logging. */
typedef enum {
    FW_REASON_NONE            = 0,
    FW_REASON_RULE_MATCH      = 1,  /* explicit rule fired          */
    FW_REASON_DEFAULT_DENY    = 2,  /* no rule matched              */
    FW_REASON_KILL_SWITCH     = 3,  /* kill switch armed            */
    FW_REASON_PARSE_ERROR     = 4,  /* malformed packet             */
    FW_REASON_BAD_LINK        = 5,  /* unknown ingress/egress       */
    FW_REASON_QUEUE_FULL      = 6,  /* egress queue full            */
    FW_REASON_FAIL_CLOSED     = 7,  /* internal fault               */
    FW_REASON_DNS_LEAK        = 8,  /* DNS to non-Tor resolver      */
    FW_REASON_FORWARD_LOOP    = 9   /* would loop back to src       */
} firewall_reason_t;

/* ==================================================================
 * Statistics counters (monotonic). Exposed to WLT-Tor-Mgr via PPC.
 * ================================================================== */
typedef struct firewall_stats {
    uint64_t rx_packets;            /* packets dequeued from guests */
    uint64_t rx_bytes;
    uint64_t tx_packets;            /* packets forwarded to egress  */
    uint64_t tx_bytes;
    uint64_t dropped;               /* total dropped                */
    uint64_t dropped_kill_switch;
    uint64_t dropped_default_deny;
    uint64_t dropped_parse_error;
    uint64_t dropped_dns_leak;
    uint64_t dropped_queue_full;
    uint64_t dropped_fail_closed;
    uint64_t rx_per_link[FW_LINK_COUNT];
    uint64_t tx_per_link[FW_LINK_COUNT];
    uint64_t last_drop_reason;      /* last FW_REASON_*             */
} firewall_stats_t;

/* ==================================================================
 * IPC protocol with WLT-Tor-Mgr (protected procedure calls on
 * channel FW_CH_WLT_TOR_MGR). Message label selects the command;
 * message registers carry arguments / results.
 * ================================================================== */
#define FW_IPC_LABEL_MASK         0xffff
#define FW_IPC_CMD_KILL_ACTIVATE  1   /* arm kill switch            */
#define FW_IPC_CMD_KILL_CLEAR     2   /* clear kill switch          */
#define FW_IPC_CMD_GET_STATUS     3   /* read stats + kill state    */
#define FW_IPC_CMD_RESET_STATS    4   /* zero counters              */
#define FW_IPC_CMD_PING           5   /* liveness probe             */

/* MR layout for GET_STATUS response. */
#define FW_IPC_MR_KILL_ACTIVE     0
#define FW_IPC_MR_RX_PACKETS      1
#define FW_IPC_MR_TX_PACKETS      2
#define FW_IPC_MR_DROPPED         3
#define FW_IPC_MR_TOR_STATE       4

/* ==================================================================
 * Kill switch interface (implemented in kill_switch.c).
 * ================================================================== */

/* Arm the kill switch. After this NO packet is forwarded. Returns
 * the previous state. Idempotent. */
bool kill_switch_activate(void);

/* Clear the kill switch. Traffic resumes subject to the rule table.
 * Returns the previous state. Idempotent. */
bool kill_switch_deactivate(void);

/* Atomically read the current kill-switch state. */
bool kill_switch_is_active(void);

/* Fail-closed hook: called when an unrecoverable fault is detected.
 * Arms the kill switch and notifies WLT-Init via the WLT-Tor-Mgr
 * channel. Never returns false. */
void kill_switch_fail_closed(void);

/* ==================================================================
 * Rule engine interface (implemented in rules.c).
 * ================================================================== */

/* Evaluate the compiled rule table against a parsed packet moving
 * from @src link toward @dst link. Returns the verdict and writes the
 * matched rule index (or -1) into @rule_idx. The kill switch is
 * checked FIRST here so callers get a single decision point. */
firewall_action_t rule_evaluate(const packet_view_t *pkt,
                                firewall_link_t src,
                                firewall_link_t dst,
                                int *rule_idx,
                                firewall_reason_t *reason);

/* Number of compiled rules (for stats / diagnostics). */
unsigned int rule_count(void);

/* ==================================================================
 * Packet parsing interface (implemented in packet.c).
 * ================================================================== */

/* Parse @len bytes at @frame into @pkt. Sets pkt->parse_ok. Always
 * safe to call on any byte stream; non-IP / truncated frames yield
 * parse_ok == false with as many fields populated as possible. */
void packet_parse(const uint8_t *frame, size_t len, packet_view_t *pkt);

/* Determine the egress link for a packet given its destination. Uses
 * the compiled-in forwarding table (destination MAC / IP). Returns
 * FW_LINK_UNKNOWN if no route matches (caller drops fail-closed). */
firewall_link_t packet_egress_link(const packet_view_t *pkt);

/* ==================================================================
 * Core PD logic (implemented in firewall.c).
 * ================================================================== */

/* Microkit entry points. */
void init(void);
void notified(microkit_channel channel);
seL4_MessageInfo_t protected(microkit_channel ch, seL4_MessageInfo_t msg);

/* Process one packet descriptor arriving on @src link. Performs parse,
 * forwarding lookup, rule evaluation, and either forwards to the
 * egress link or drops. Updates stats. */
void firewall_handle_packet(net_link_t *link, firewall_link_t src,
                            net_buff_desc_t *desc);

/* Drain all RX descriptors available on @src link. Returns the number
 * processed. */
unsigned int firewall_drain_rx(net_link_t *link, firewall_link_t src);

/* Drain recycled TX buffers returned by the NIC driver (so the
 * firewall can reuse them for new egress). */
void firewall_drain_nic_reclaim(void);

/* Forward @desc (already copied into the egress pool) to @dst link.
 * Notifies the peer. Returns 0 on success, -1 if the TX queue is
 * full (caller drops). */
int firewall_forward(net_link_t *dst_link, net_buff_desc_t *desc);

/* Drop @desc on @src link, recording @reason. Recycles the buffer
 * back to the peer's free ring. */
void firewall_drop(net_link_t *src_link, net_buff_desc_t *desc,
                   firewall_reason_t reason);

/* IPC handlers. */
seL4_MessageInfo_t firewall_handle_ipc(microkit_channel ch,
                                       seL4_MessageInfo_t msg);

/* Minimal debug logging - writes through microkit_dbg_putc. No
 * stdio, no allocation. Compiled out unless FW_DEBUG is defined. */
#ifdef FW_DEBUG
void fw_log(const char *fmt, ...);
void fw_log_hex(uint64_t v);
void fw_log_dec(uint64_t v);
#else
static inline void fw_log(const char *fmt, ...) { (void)fmt; }
static inline void fw_log_hex(uint64_t v) { (void)v; }
static inline void fw_log_dec(uint64_t v) { (void)v; }
#endif

/* Global state accessors (for tests / diagnostics). */
const firewall_stats_t *firewall_stats_get(void);
net_link_t *firewall_link(firewall_link_t id);

#endif /* WLTIOS_FIREWALL_H */
