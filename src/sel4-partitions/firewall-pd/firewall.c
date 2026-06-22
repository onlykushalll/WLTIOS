/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * WLTIOS - seL4 Firewall Protection Domain
 * firewall.c
 *
 * Main PD logic: Microkit entry points, sDDF ring plumbing, the packet
 * event loop, and the IPC handler for WLT-Tor-Mgr.
 *
 * Lifecycle
 * ---------
 *   init()      - runs once at PD boot. Wires up the three sDDF links
 *                 (Main Linux, Tor VM, NIC driver) from the shared
 *                 memory regions the SDF mapped into this PD, primes
 *                 the free rings, and arms nothing.
 *   notified()  - runs whenever a peer signals a channel. We drain the
 *                 relevant RX ring(s) to completion, forwarding or
 *                 dropping each packet.
 *   protected() - runs on PPCs from WLT-Tor-Mgr (kill switch, status).
 *
 * Buffer model
 * ------------
 *  - Each link owns a data pool and four rings (rx_used, rx_free,
 *    tx_used, tx_free) shared with its peer.
 *  - On RX we dequeue from rx_used, process the packet, and return the
 *    descriptor to rx_free (zero-copy: we never copy the payload when
 *    dropping). On forward we copy the payload into the egress link's
 *    tx pool (the firewall is a store-and-forward middlebox, not a
 *    pure cut-through, because the two links have independent pools).
 *  - Recycled TX buffers from the NIC driver are returned to us on
 *    tx_free and we top up the free pool for the next transmit.
 *
 * All paths are fail-closed: any error drops the packet and, for
 * serious faults, arms the kill switch.
 */
#include "firewall.h"
#include "rules.h"

/* ------------------------------------------------------------------ */
/* Shared memory region symbols.                                       */
/*                                                                      */
/* These are provided by the Microkit linker script / SDF memory       */
/* regions. Each link gets four rings + two pools. The layout below is */
/* documented in docs/specs/firewall-pd.md and mirrored by the SDF.    */
/* ------------------------------------------------------------------ */

/* Each ring occupies one FW_RING_SIZE-slot block. The build places
 * rings contiguously per link in the order: rx_used, rx_free, tx_used,
 * tx_free, then the rx pool, then the tx pool. */
#define FW_RING_BYTES   (sizeof(net_ring_t))

extern char fw_shmem_main_linux[];   /* region for Main Linux link   */
extern char fw_shmem_tor_vm[];       /* region for Tor VM link       */
extern char fw_shmem_nic_driver[];   /* region for NIC driver link   */

/* Internal kill-switch fault hooks defined in kill_switch.c. */
extern void kill_switch_nic_dead(void);
extern void kill_switch_ipc_failed(void);
extern bool kill_switch_fail_closed_latched(void);
extern uint32_t kill_switch_last_reason(void);
extern uint64_t kill_switch_transitions(void);

/* ------------------------------------------------------------------ */
/* PD-global state.                                                    */
/* ------------------------------------------------------------------ */
static net_link_t g_links[FW_LINK_COUNT];
static firewall_stats_t g_stats;

/* A few packets are allowed to be in flight to the NIC before we
 * consider it unresponsive. */
#define FW_NIC_TX_WATERMARK   (FW_RING_SIZE / 2)
#define FW_NIC_STALL_LIMIT    4096   /* packets forwarded with no    */
                                     /* reclaim before we panic      */
static uint64_t g_nic_in_flight;
static uint64_t g_nic_since_reclaim;
static uint32_t g_nic_tx_tail_last;   /* last seen NIC tx_free producer idx */

/* ------------------------------------------------------------------ */
/* Ring operations (inline helpers, no locking - SPSC).                */
/* ------------------------------------------------------------------ */
static inline bool ring_empty(const net_ring_t *r)
{
    return r->tail == r->head;
}

static inline bool ring_full(const net_ring_t *r)
{
    return (uint32_t)(r->tail - r->head) >= r->size;
}

static inline int ring_enqueue(net_ring_t *r, const net_buff_desc_t *d)
{
    if (ring_full(r)) {
        return -1;
    }
    r->buffers[r->tail & FW_RING_MASK] = *d;
    /* Publish the descriptor before bumping the producer index so the
     * consumer never sees a half-written entry. */
    __asm__ volatile("dmb ishst" ::: "memory");
    r->tail++;
    return 0;
}

static inline int ring_dequeue(net_ring_t *r, net_buff_desc_t *d)
{
    if (ring_empty(r)) {
        return -1;
    }
    /* Acquire the data before advancing the consumer index. */
    *d = r->buffers[r->head & FW_RING_MASK];
    __asm__ volatile("dmb ishld" ::: "memory");
    r->head++;
    return 0;
}

static inline uint8_t *pool_ptr(net_link_t *link, const net_buff_desc_t *d,
                                bool rx)
{
    uint8_t *base = rx ? link->rx_pool : link->tx_pool;
    size_t   sz   = rx ? link->rx_pool_size : link->tx_pool_size;
    if (d->offset + d->len > sz) {
        return NULL;            /* out of bounds - corrupt descriptor */
    }
    return base + d->offset;
}

/* ------------------------------------------------------------------ */
/* Link initialisation.                                                */
/* ------------------------------------------------------------------ */
/*
 * Lay out one shared region into rings + pools. The region is laid
 * out as:
 *
 *   [ rx_used | rx_free | tx_used | tx_free | rx_pool | tx_pool ]
 *
 * The peer PD must agree on this layout (it does - see the SDF +
 * peer configuration). We only initialise our view of the rings; the
 * free rings are primed by the producer side (the peer for rx_free,
 * us for tx_free).
 */
static void link_init(net_link_t *link, firewall_link_t id,
                      char *region, size_t region_size,
                      microkit_channel ch, const char *name)
{
    /* Defensive: a misconfigured SDF could give us a tiny region. */
    if (region_size < 4 * FW_RING_BYTES + 2 * FW_POOL_SIZE) {
        kill_switch_fail_closed();
        return;
    }

    char *p = region;
    link->rx_used = (net_ring_t *)p;            p += FW_RING_BYTES;
    link->rx_free = (net_ring_t *)p;            p += FW_RING_BYTES;
    link->tx_used = (net_ring_t *)p;            p += FW_RING_BYTES;
    link->tx_free = (net_ring_t *)p;            p += FW_RING_BYTES;
    link->rx_pool = (uint8_t *)p;               p += FW_POOL_SIZE;
    link->tx_pool = (uint8_t *)p;               p += FW_POOL_SIZE;

    link->rx_pool_size = FW_POOL_SIZE;
    link->tx_pool_size = FW_POOL_SIZE;
    link->ch   = ch;
    link->name = name;

    /* Size the rings (the peer relies on the same constant). */
    link->rx_used->size = FW_RING_SIZE;
    link->rx_free->size = FW_RING_SIZE;
    link->tx_used->size = FW_RING_SIZE;
    link->tx_free->size = FW_RING_SIZE;

    /* We own the producer side of: rx_free (we return buffers to the
     * peer), tx_used (we send buffers to the peer). We own the
     * consumer side of: rx_used, tx_free. head/tail start at 0 on
     * both sides at boot - that is the sDDF convention. */
    (void)id;
}

/* Prime our TX free pool with buffer descriptors so we have something
 * to copy forwarded packets into. The pool is carved into MTU-sized
 * slots. */
static void link_prime_tx_free(net_link_t *link)
{
    if (link->tx_pool == NULL) {
        return;
    }
    uint32_t slots = (uint32_t)(link->tx_pool_size / FW_MTU);
    if (slots > FW_RING_SIZE) {
        slots = FW_RING_SIZE;
    }
    for (uint32_t i = 0; i < slots; i++) {
        net_buff_desc_t d = {
            .offset = (uint64_t)i * FW_MTU,
            .len    = 0,
            .flags  = FW_BUF_TX,
            .cookie = 0,
        };
        if (ring_enqueue(link->tx_free, &d) != 0) {
            break;              /* ring full - fine */
        }
    }
}

net_link_t *firewall_link(firewall_link_t id)
{
    if ((unsigned int)id >= FW_LINK_COUNT) {
        return NULL;
    }
    return &g_links[id];
}

const firewall_stats_t *firewall_stats_get(void)
{
    return &g_stats;
}

/* ------------------------------------------------------------------ */
/* Microkit entry points.                                              */
/* ------------------------------------------------------------------ */
void init(void)
{
    /* Zero global state. */
    for (unsigned int i = 0; i < FW_LINK_COUNT; i++) {
        g_links[i].rx_used = NULL;
        g_links[i].rx_free = NULL;
        g_links[i].tx_used = NULL;
        g_links[i].tx_free = NULL;
        g_links[i].rx_pool = NULL;
        g_links[i].tx_pool = NULL;
    }
    for (unsigned int i = 0;
         i < sizeof(g_stats) / sizeof(uint64_t); i++) {
        ((uint64_t *)&g_stats)[i] = 0;
    }
    g_nic_in_flight = 0;
    g_nic_since_reclaim = 0;
    g_nic_tx_tail_last = 0;

    fw_log("firewall: init\n");

    /* Wire up the three links. Region sizes come from the SDF; we
     * assume each is at least large enough for the layout above. */
    link_init(&g_links[FW_LINK_MAIN_LINUX], FW_LINK_MAIN_LINUX,
              fw_shmem_main_linux, FW_POOL_SIZE * 2 + FW_RING_BYTES * 4,
              FW_CH_MAIN_LINUX, "main-linux");
    link_init(&g_links[FW_LINK_TOR_VM], FW_LINK_TOR_VM,
              fw_shmem_tor_vm, FW_POOL_SIZE * 2 + FW_RING_BYTES * 4,
              FW_CH_TOR_VM, "tor-vm");
    link_init(&g_links[FW_LINK_NIC_DRIVER], FW_LINK_NIC_DRIVER,
              fw_shmem_nic_driver, FW_POOL_SIZE * 2 + FW_RING_BYTES * 4,
              FW_CH_NIC_DRIVER, "nic-driver");

    /* Prime our TX free pools so we can accept forwards. */
    link_prime_tx_free(&g_links[FW_LINK_MAIN_LINUX]);
    link_prime_tx_free(&g_links[FW_LINK_TOR_VM]);
    link_prime_tx_free(&g_links[FW_LINK_NIC_DRIVER]);

    /* We start with the kill switch OFF. If WLT-Init wants the device
     * to boot dark, it sends KILL_ACTIVATE before raising the guests. */
    fw_log("firewall: links ready, rules=%u\n", rule_count());
}

void notified(microkit_channel channel)
{
    switch (channel) {
    case FW_CH_MAIN_LINUX:
        firewall_drain_rx(&g_links[FW_LINK_MAIN_LINUX],
                          FW_LINK_MAIN_LINUX);
        break;
    case FW_CH_TOR_VM:
        firewall_drain_rx(&g_links[FW_LINK_TOR_VM],
                          FW_LINK_TOR_VM);
        break;
    case FW_CH_NIC_DRIVER:
        /* The NIC driver signals us either because it has returned
         * recycled TX buffers, or because it has RX packets from the
         * wire. We handle both. */
        firewall_drain_nic_reclaim();
        firewall_drain_rx(&g_links[FW_LINK_NIC_DRIVER],
                          FW_LINK_NIC_DRIVER);
        /* Reclaim may have freed space - re-kick egress if we had
         * backpressure. */
        break;
    case FW_CH_WLT_TOR_MGR:
        /* Async notifications from WLT-Tor-Mgr are informational; the
         * authoritative commands arrive via protected(). */
        break;
    default:
        /* Unknown channel - ignore. seL4 guarantees we cannot be
         * notified on a channel we do not have. */
        break;
    }

    /* Cooperative draining: a peer may have enqueued many packets in
     * one burst. Loop until all RX rings are empty so we do not leave
     * latency on the table. */
    for (;;) {
        bool any = false;
        if (!ring_empty(g_links[FW_LINK_MAIN_LINUX].rx_used)) {
            firewall_drain_rx(&g_links[FW_LINK_MAIN_LINUX],
                              FW_LINK_MAIN_LINUX);
            any = true;
        }
        if (!ring_empty(g_links[FW_LINK_TOR_VM].rx_used)) {
            firewall_drain_rx(&g_links[FW_LINK_TOR_VM],
                              FW_LINK_TOR_VM);
            any = true;
        }
        if (!ring_empty(g_links[FW_LINK_NIC_DRIVER].rx_used)) {
            firewall_drain_rx(&g_links[FW_LINK_NIC_DRIVER],
                              FW_LINK_NIC_DRIVER);
            any = true;
        }
        if (!ring_empty(g_links[FW_LINK_NIC_DRIVER].tx_free)) {
            firewall_drain_nic_reclaim();
            any = true;
        }
        if (!any) {
            break;
        }
    }
}

seL4_MessageInfo_t protected(microkit_channel ch, seL4_MessageInfo_t msg)
{
    return firewall_handle_ipc(ch, msg);
}

/* ------------------------------------------------------------------ */
/* Packet handling.                                                    */
/* ------------------------------------------------------------------ */

/* Recycle an RX descriptor back to the peer's free ring. */
static void recycle_rx(net_link_t *link, net_buff_desc_t *d)
{
    d->len   = 0;
    d->flags = FW_BUF_RX;
    /* If the free ring is full the peer has not been draining; we
     * simply drop the recycle and the peer will re-primed on its
     * side. The packet itself is already accounted for. */
    if (ring_enqueue(link->rx_free, d) != 0) {
        fw_log("firewall: rx_free full on %s\n", link->name);
    }
}

void firewall_drop(net_link_t *src_link, net_buff_desc_t *desc,
                   firewall_reason_t reason)
{
    g_stats.dropped++;
    g_stats.last_drop_reason = (uint64_t)reason;
    switch (reason) {
    case FW_REASON_KILL_SWITCH:   g_stats.dropped_kill_switch++;   break;
    case FW_REASON_DEFAULT_DENY:  g_stats.dropped_default_deny++;  break;
    case FW_REASON_PARSE_ERROR:   g_stats.dropped_parse_error++;   break;
    case FW_REASON_DNS_LEAK:      g_stats.dropped_dns_leak++;      break;
    case FW_REASON_QUEUE_FULL:    g_stats.dropped_queue_full++;    break;
    case FW_REASON_FAIL_CLOSED:   g_stats.dropped_fail_closed++;   break;
    default: break;
    }

    fw_log("firewall: DROP(%u) on %s len=%u\n",
           (unsigned)reason, src_link ? src_link->name : "?",
           desc ? desc->len : 0);

    if (src_link != NULL && desc != NULL) {
        recycle_rx(src_link, desc);
    }
}

int firewall_forward(net_link_t *dst_link, net_buff_desc_t *desc)
{
    /* NOTE: @desc must already reference a buffer that has been filled
     * in the egress link's TX pool (firewall_handle_packet performs
     * the copy before calling us). We only publish the descriptor to
     * the peer and do bookkeeping - no allocation, no copy here. */
    if (dst_link == NULL || desc == NULL || dst_link->tx_used == NULL) {
        return -1;
    }

    if (ring_enqueue(dst_link->tx_used, desc) != 0) {
        /* tx_used full - return the buffer to the free pool and let
         * the caller drop the source packet. */
        desc->len = 0;
        ring_enqueue(dst_link->tx_free, desc);
        return -1;
    }

    /* Tell the peer there is work to do. */
    microkit_notify(dst_link->ch);

    /* NIC-driver health bookkeeping. We count forwards here and look
     * for corresponding progress on the NIC's tx_free producer index
     * in firewall_drain_nic_reclaim(). */
    if (dst_link == &g_links[FW_LINK_NIC_DRIVER]) {
        g_nic_in_flight++;
        g_nic_since_reclaim++;
        if (g_nic_since_reclaim > FW_NIC_STALL_LIMIT) {
            fw_log("firewall: NIC stall, fail-closed\n");
            kill_switch_nic_dead();
        }
    }
    return 0;
}

void firewall_handle_packet(net_link_t *link, firewall_link_t src,
                            net_buff_desc_t *desc)
{
    if (link == NULL || desc == NULL) {
        return;
    }

    g_stats.rx_packets++;
    g_stats.rx_bytes += desc->len;
    g_stats.rx_per_link[src]++;

    /* Resolve the packet payload pointer in the ingress pool. */
    const uint8_t *frame = pool_ptr(link, desc, true);
    if (frame == NULL || desc->len == 0 || desc->len > FW_MAX_PKT_LEN) {
        firewall_drop(link, desc, FW_REASON_PARSE_ERROR);
        return;
    }

    /* Parse headers (zero-copy: pointers into the pool). */
    packet_view_t pkt;
    packet_parse(frame, desc->len, &pkt);

    /* Decide where the packet is going. */
    firewall_link_t dst = packet_egress_link(&pkt);
    if (dst == FW_LINK_UNKNOWN) {
        firewall_drop(link, desc, FW_REASON_BAD_LINK);
        return;
    }

    /* Evaluate the policy (kill switch + rules). */
    int rule_idx = -1;
    firewall_reason_t reason = FW_REASON_NONE;
    firewall_action_t action = rule_evaluate(&pkt, src, dst,
                                             &rule_idx, &reason);

    if (action != FW_ACTION_ALLOW) {
        firewall_drop(link, desc, reason);
        return;
    }

    /* ALLOWED: copy the payload into the egress TX pool, then publish
     * it via firewall_forward(). */
    net_link_t *egress = &g_links[dst];

    net_buff_desc_t tx;
    if (ring_dequeue(egress->tx_free, &tx) != 0) {
        firewall_drop(link, desc, FW_REASON_QUEUE_FULL);
        return;
    }
    uint8_t *dst_buf = pool_ptr(egress, &tx, false);
    if (dst_buf == NULL) {
        tx.len = 0;
        ring_enqueue(egress->tx_free, &tx);
        firewall_drop(link, desc, FW_REASON_FAIL_CLOSED);
        return;
    }

    /* Bounded copy. desc->len was already checked <= FW_MAX_PKT_LEN
     * and TX slots are FW_MTU sized, so this always fits. */
    for (uint16_t i = 0; i < desc->len; i++) {
        dst_buf[i] = frame[i];
    }
    tx.len    = desc->len;
    tx.flags  = FW_BUF_TX;
    tx.cookie = desc->cookie;

    if (firewall_forward(egress, &tx) != 0) {
        /* Egress tx_used full. firewall_forward already returned the
         * buffer to tx_free; drop the source packet. */
        firewall_drop(link, desc, FW_REASON_QUEUE_FULL);
        return;
    }

    /* Success: the source buffer is no longer needed. */
    recycle_rx(link, desc);

    g_stats.tx_packets++;
    g_stats.tx_bytes += tx.len;
    g_stats.tx_per_link[dst]++;

    fw_log("firewall: FWD %s->%s len=%u rule=%d\n",
           link->name, egress->name, tx.len, rule_idx);
}

unsigned int firewall_drain_rx(net_link_t *link, firewall_link_t src)
{
    if (link == NULL || link->rx_used == NULL) {
        return 0;
    }
    unsigned int n = 0;
    net_buff_desc_t d;
    while (ring_dequeue(link->rx_used, &d) == 0) {
        firewall_handle_packet(link, src, &d);
        n++;
    }
    return n;
}

void firewall_drain_nic_reclaim(void)
{
    /* The NIC driver returns transmitted buffers by advancing the
     * producer index of our shared tx_free ring. We do NOT move those
     * descriptors: they are already free and the forward path consumes
     * them directly. This hook is purely for NIC-health accounting.
     *
     * If the NIC's tx_free producer index advanced since we last
     * looked, the driver is alive and recycling - reset the stall
     * counter and decay the in-flight estimate by the number of
     * buffers returned. */
    net_link_t *nic = &g_links[FW_LINK_NIC_DRIVER];
    if (nic->tx_free == NULL) {
        return;
    }

    uint32_t cur_tail = nic->tx_free->tail;
    if (cur_tail != g_nic_tx_tail_last) {
        uint32_t delta = cur_tail - g_nic_tx_tail_last;
        g_nic_tx_tail_last = cur_tail;
        if (g_nic_in_flight > delta) {
            g_nic_in_flight -= delta;
        } else {
            g_nic_in_flight = 0;
        }
        g_nic_since_reclaim = 0;
    }
}

/* ------------------------------------------------------------------ */
/* IPC with WLT-Tor-Mgr.                                               */
/* ------------------------------------------------------------------ */
seL4_MessageInfo_t firewall_handle_ipc(microkit_channel ch,
                                       seL4_MessageInfo_t msg)
{
    if (ch != FW_CH_WLT_TOR_MGR) {
        /* We only accept PPCs from the manager. Anything else is a
         * protocol violation - fail-closed. */
        kill_switch_ipc_failed();
        return microkit_msginfo_new(0, 0);
    }

    seL4_Word label = microkit_msginfo_get_label(msg);
    seL4_Word cmd   = label & FW_IPC_LABEL_MASK;

    switch (cmd) {
    case FW_IPC_CMD_KILL_ACTIVATE: {
        bool prev = kill_switch_activate();
        microkit_mr_set(FW_IPC_MR_KILL_ACTIVE, 1);
        fw_log("firewall: IPC kill-activate (was=%d)\n", prev ? 1 : 0);
        return microkit_msginfo_new(cmd, 1);
    }
    case FW_IPC_CMD_KILL_CLEAR: {
        bool prev = kill_switch_deactivate();
        microkit_mr_set(FW_IPC_MR_KILL_ACTIVE,
                        kill_switch_is_active() ? 1 : 0);
        fw_log("firewall: IPC kill-clear (was=%d)\n", prev ? 1 : 0);
        return microkit_msginfo_new(cmd, 1);
    }
    case FW_IPC_CMD_GET_STATUS: {
        microkit_mr_set(FW_IPC_MR_KILL_ACTIVE,
                        kill_switch_is_active() ? 1 : 0);
        microkit_mr_set(FW_IPC_MR_RX_PACKETS,
                        (seL4_Word)g_stats.rx_packets);
        microkit_mr_set(FW_IPC_MR_TX_PACKETS,
                        (seL4_Word)g_stats.tx_packets);
        microkit_mr_set(FW_IPC_MR_DROPPED,
                        (seL4_Word)g_stats.dropped);
        microkit_mr_set(FW_IPC_MR_TOR_STATE,
                        (seL4_Word)kill_switch_last_reason());
        return microkit_msginfo_new(cmd, 5);
    }
    case FW_IPC_CMD_RESET_STATS: {
        for (unsigned int i = 0;
             i < sizeof(g_stats) / sizeof(uint64_t); i++) {
            ((uint64_t *)&g_stats)[i] = 0;
        }
        return microkit_msginfo_new(cmd, 0);
    }
    case FW_IPC_CMD_PING: {
        microkit_mr_set(0, FW_PD_ID);
        return microkit_msginfo_new(cmd, 1);
    }
    default:
        /* Unknown command - fail-closed. */
        kill_switch_ipc_failed();
        return microkit_msginfo_new(0, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Minimal debug logging.                                              */
/*                                                                      */
/* No stdio: we format by hand and emit through microkit_dbg_putc.     */
/* Compiled in only when FW_DEBUG is defined.                          */
/* ------------------------------------------------------------------ */
#ifdef FW_DEBUG

static void fw_putc(char c)
{
    microkit_dbg_putc(c);
}

static void fw_puts(const char *s)
{
    while (*s) {
        fw_putc(*s++);
    }
}

void fw_log_hex(uint64_t v)
{
    const char *hex = "0123456789abcdef";
    fw_puts("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nib = (uint8_t)((v >> i) & 0xf);
        if (nib || started || i == 0) {
            fw_putc(hex[nib]);
            started = 1;
        }
    }
}

void fw_log_dec(uint64_t v)
{
    char buf[24];
    int i = 0;
    if (v == 0) {
        fw_putc('0');
        return;
    }
    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0) {
        fw_putc(buf[--i]);
    }
}

/* A very small printf subset: %s %u %d %x %c %p. No width/flags. */
void fw_log(const char *fmt, ...)
{
    /* Variadic handling without stdarg would be ugly; microkit
     * provides <stdarg.h> via the toolchain. */
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            fw_putc(*p);
            continue;
        }
        p++;
        switch (*p) {
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            fw_puts(s ? s : "(null)");
            break;
        }
        case 'u': {
            uint64_t v = (uint64_t)__builtin_va_arg(ap, unsigned long);
            fw_log_dec(v);
            break;
        }
        case 'd': {
            long v = __builtin_va_arg(ap, long);
            if (v < 0) {
                fw_putc('-');
                fw_log_dec((uint64_t)(-v));
            } else {
                fw_log_dec((uint64_t)v);
            }
            break;
        }
        case 'x': case 'p': {
            uint64_t v = (uint64_t)__builtin_va_arg(ap, unsigned long);
            fw_log_hex(v);
            break;
        }
        case 'c': {
            int c = __builtin_va_arg(ap, int);
            fw_putc((char)c);
            break;
        }
        case '%':
            fw_putc('%');
            break;
        default:
            fw_putc('%');
            fw_putc(*p);
            break;
        }
    }
    __builtin_va_end(ap);
}

#endif /* FW_DEBUG */
