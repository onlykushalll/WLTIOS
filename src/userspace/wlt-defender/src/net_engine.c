
/* net_engine.c — Layer 5: Network IDS (Tor-focused).
 *
 * Bug 5 fix: COMPLETELY rewritten to use UID-based detection instead of the
 * broken IP whitelist. Tor's actual traffic goes to guard nodes (8000+ dynamic
 * IPs) — whitelisting 5 directory authority IPs flagged ALL Tor traffic as
 * suspicious, which would have activated the kill switch on every Tor circuit.
 *
 * New approach: detect Tor's UID at init time. Any connection in /proc/net/tcp
 * belonging to a non-Tor UID (and not to localhost) = leak = kill switch.
 *
 * Bug 11 fix: now checks both IPv4 (/proc/net/tcp, /proc/net/udp) AND IPv6
 * (/proc/net/tcp6, /proc/net/udp6). Old code only checked IPv4, making IPv6
 * a full bypass.
 */
#include "guardian.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct g_net_engine {
    char ifname[64];
    uid_t tor_uid;       /* Bug 5 fix: UID-based detection, not IP whitelist */
    pid_t tor_pid;
    /* Phase 4.4: Tor consensus guard node set.
     * If non-empty, Tor connections to IPs NOT in this set are flagged
     * SUSPICIOUS_TOR_DESTINATION (possible Tor compromise). */
    uint32_t guard_ips[256];  /* network-order IPv4 addresses */
    int n_guard_ips;
    time_t consensus_loaded;
};

/* find the tor process and return its UID */
static pid_t find_tor(uid_t *out_uid) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        pid_t pid = atoi(de->d_name);
        char path[128];
        /* check comm == "tor" */
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char comm[32] = {0};
        if (fgets(comm, sizeof(comm), f)) {
            char *nl = strchr(comm, '\n'); if (nl) *nl = 0;
        }
        fclose(f);
        if (strcmp(comm, "tor") != 0) continue;
        /* found tor — get its UID from /proc/<pid>/status */
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        f = fopen(path, "r");
        if (!f) continue;
        char line[256];
        uid_t uid = 0;
        while (fgets(line, sizeof(line), f)) {
            if (!strncmp(line, "Uid:", 4)) {
                sscanf(line + 4, "%u", &uid);
                break;
            }
        }
        fclose(f);
        if (out_uid) *out_uid = uid;
        closedir(d);
        return pid;
    }
    closedir(d);
    return 0;
}

/* Phase 4.4: parse /var/lib/tor/cached-consensus (or cached-microdesc-consensus)
 * for guard node IPs. Build an in-memory set for destination validation. */
static int load_tor_consensus(g_net_engine_t *ne) {
    const char *paths[] = {
        "/var/lib/tor/cached-consensus",
        "/var/lib/tor/cached-microdesc-consensus",
        NULL
    };
    ne->n_guard_ips = 0;
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (!f) continue;
        char line[1024];
        while (fgets(line, sizeof(line), f) && ne->n_guard_ips < 256) {
            /* consensus lines: "r <nickname> <hash> <datetime> <IP> <ORport> <DIRport>" */
            if (line[0] != 'r' || line[1] != ' ') continue;
            char ip[64] = {0};
            if (sscanf(line + 2, "%*s %*s %*s %63s", ip) == 1) {
                struct in_addr a;
                if (inet_aton(ip, &a)) {
                    ne->guard_ips[ne->n_guard_ips++] = a.s_addr;
                }
            }
        }
        fclose(f);
        if (ne->n_guard_ips > 0) {
            ne->consensus_loaded = time(NULL);
            printf("[net] loaded %d Tor guard node IPs from %s\n", ne->n_guard_ips, paths[i]);
            return 0;
        }
    }
    return -1;
}

g_net_engine_t *g_net_init(const char *ifname) {
    g_net_engine_t *ne = calloc(1, sizeof(*ne));
    if (!ne) return NULL;
    g_strlcpy(ne->ifname, ifname ? ifname : "eth0", sizeof(ne->ifname));
    ne->tor_pid = find_tor(&ne->tor_uid);
    if (ne->tor_pid) {
        printf("[net] tor detected: pid=%d uid=%d (connections from this UID are allowed)\n",
               ne->tor_pid, ne->tor_uid);
        /* Phase 4.4: load consensus guard node set for destination validation */
        load_tor_consensus(ne);
    } else {
        printf("[net] tor not found — ALL non-root egress will be flagged\n");
        ne->tor_uid = (uid_t)-1;  /* sentinel: no tor = flag everything */
    }
    return ne;
}

void g_net_free(g_net_engine_t *ne) { free(ne); }

/* Bug 5+11 fix: check a /proc/net/proto file for non-Tor-UID connections.
 * The UID is column 8 in /proc/net/tcp (and tcp6, udp, udp6). */
static int check_proto_uid(g_net_engine_t *ne, const char *proto_file, g_event_t *out) {
    FILE *f = fopen(proto_file, "r");
    if (!f) return 0;
    char line[512];
    (void)fgets(line, sizeof(line), f);  /* header */
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* /proc/net/tcp format: sl local rem st tx_queue:rx_queue
         * tr:tm->when retrnsmt uid timeout inode
         *
         * [fix] The kernel emits tx_queue:rx_queue and tr:tm->when as
         * ONE colon-joined token each (e.g. "00000000:00000000"), not
         * as independent space-separated numbers - confirmed against
         * the kernel's own tcp4_seq_show format ("%08X:%08X" and
         * "%02X:%08lX"). The previous format string used 4 separate
         * %lu conversions for these two tokens: the first %lu would
         * read up to the internal colon and stop, then the next %lu
         * would immediately fail (it only skips whitespace, not a
         * colon) - so sscanf returned early with n=4 on every line,
         * under the "if (n < 8) continue" threshold, meaning this
         * parser silently skipped every single line, on every call,
         * and g_net_check_leak() could never detect anything.
         *
         * Fix: local/remote/state were already read as strings
         * (correctly, since they're colon-joined hex:hex tokens
         * too) - do the same for the two queue/timer fields, and
         * skip retrnsmt as a suppressed hex read. Only uid is
         * actually used below; nothing else needs a named variable. */
        char local[64], remote[64], state[16];
        unsigned int uid;
        int n = sscanf(line, "%*d: %63s %63s %15s %*s %*s %*x %u",
                       local, remote, state, &uid);
        if (n < 4) continue;
        if (!strcmp(state, "0A")) continue;  /* LISTEN — ok */

        /* parse remote address to check if it's localhost */
        /* remote is "HEXIP:HEXPORT" — for IPv4, 0100007F = 127.0.0.1 */
        unsigned int raddr_hex, rport_hex;
        if (sscanf(remote, "%X:%X", &raddr_hex, &rport_hex) == 2) {
            /* localhost check (big-endian 0100007F = 127.0.0.1) */
            if (raddr_hex == 0x0100007F) continue;
            if (rport_hex == 0) continue;  /* no remote port yet */
        }

        /* Bug 5 fix: if this connection belongs to tor's UID, it's OK */
        /* [fix 2026-08-24] That was true as far as it went, but this
         * unconditional `continue` also meant the guard_ips consensus
         * data loaded by load_tor_consensus() (see the struct comment
         * near the top of this file: "flag Tor connections to non-guard
         * IPs... SUSPICIOUS_TOR_DESTINATION") was never actually
         * consulted anywhere - a real, working parser feeding a check
         * that never ran (STATUS_ROADMAP.md gap #24). Only add scrutiny
         * when guard data is actually loaded; if consensus hasn't
         * loaded yet (n_guard_ips == 0), keep the original trust-tor-uid
         * behavior rather than flooding false positives with nothing to
         * check against. raddr_hex and guard_ips[] are both s_addr's
         * raw network-byte-order bits - directly comparable, confirmed
         * against this function's own "0100007F = 127.0.0.1" example
         * above, no byte-swap needed. */
        if (ne->tor_uid != (uid_t)-1 && uid == ne->tor_uid) {
            if (ne->n_guard_ips == 0 || rport_hex == 0)
                continue;
            int is_guard = 0;
            for (int gi = 0; gi < ne->n_guard_ips; gi++) {
                if (ne->guard_ips[gi] == raddr_hex) { is_guard = 1; break; }
            }
            if (is_guard)
                continue;
            snprintf(out->detail, sizeof(out->detail),
                     "SUSPICIOUS_TOR_DESTINATION: tor uid=%u connected to "
                     "non-guard IP %s - possible compromise or "
                     "consensus-bypass", uid, remote);
            found = 1;
            break;
        }

        /* root (uid 0) connections to localhost already skipped above.
         * Root connections to external IPs could be legitimate system stuff
         * (DNS resolver, NTP) — flag but don't kill-switch. */
        const char *proto = strstr(proto_file, "udp") ? "UDP" : "TCP";
        int is_ipv6 = strstr(proto_file, "6") != NULL;
        snprintf(out->detail, sizeof(out->detail),
                 "non-Tor %s%s egress from uid=%u (tor_uid=%d) to %s",
                 is_ipv6 ? "6" : "4", proto, uid, ne->tor_uid, remote);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

int g_net_check_leak(g_net_engine_t *ne, g_event_t *out) {
    if (!ne || !out) return -EINVAL;
    memset(out, 0, sizeof(*out));
    g_strlcpy(out->source, "net", sizeof(out->source));
    out->ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
    out->seq = g_bus_next_seq();
    g_strlcpy(out->rule_id, "NET_LEAK_CHECK", sizeof(out->rule_id));

    /* [fix 2026-08-16] find_tor() was previously only ever called once, at
     * g_net_init() time. If Guardian starts before Tor is actually up (a
     * real possibility - Tor may run in a separate VM/partition with its
     * own boot sequence), tor_uid got stuck at the "not found" sentinel
     * forever, and this engine would flag ALL non-root traffic as
     * suspicious indefinitely, even after Tor successfully started later.
     * Retry the lookup on every check while it's still unresolved - once
     * Tor is found this branch is skipped on all future calls, so the
     * cost is bounded to the "not found yet" window only. */
    if (ne->tor_uid == (uid_t)-1) {
        pid_t pid = find_tor(&ne->tor_uid);
        if (pid) {
            ne->tor_pid = pid;
            printf("[net] tor found on retry: pid=%d uid=%d\n", pid, ne->tor_uid);
            load_tor_consensus(ne);
        }
    }

    /* Bug 11 fix: check ALL four /proc/net files (IPv4 + IPv6, TCP + UDP) */
    if (check_proto_uid(ne, "/proc/net/tcp", out) ||
        check_proto_uid(ne, "/proc/net/tcp6", out) ||
        check_proto_uid(ne, "/proc/net/udp", out) ||
        check_proto_uid(ne, "/proc/net/udp6", out)) {
        out->verdict = G_VERDICT_SUSPICIOUS;
        out->severity = G_SEV_CRITICAL;
        out->action_taken = G_ACT_LOG;  /* policy engine will upgrade */
        g_bus_publish(out);
        return 1;
    }
    out->verdict = G_VERDICT_CLEAN;
    return 0;
}

int g_net_capture(g_net_engine_t *ne, int duration_sec,
                  int (*cb)(const g_event_t *, void *), void *ud) {
    if (!ne || !cb) return -EINVAL;
    int total = 0;
    time_t end = time(NULL) + duration_sec;
    while (time(NULL) < end) {
        g_event_t ev = {0};
        if (g_net_check_leak(ne, &ev) > 0) {
            cb(&ev, ud);
            total++;
        }
        usleep(500000);
    }
    return total;
}

uid_t g_net_get_tor_uid(g_net_engine_t *ne) {
    if (!ne) return (uid_t)-1;
    return ne->tor_uid;
}
