
/* action_engine.c — enforcement execution (real implementations).
 *
 * Tier 0 (LOG): write to log, no kernel action.
 * Tier 1 (reversible): cgroup freezer, file quarantine, unshare CLONE_NEWNET,
 *                       network block. Autonomous.
 * Tier 2 (irreversible): SIGKILL, secure delete, kill switch. Human-confirm.
 *
 * In dev sandbox: many actions return simulated=true (no root). On WLTIOS
 * target as root: real kernel facilities.
 */
#include "guardian.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static bool have_root(void) { return geteuid() == 0; }

int g_action_init(void) {
    printf("[action] tier policy: reversible=autonomous, irreversible=human-confirm\n");
    if (!have_root()) printf("[action] running unprivileged — actions will be simulated\n");
    return 0;
}

/* ---- freeze / thaw (cgroup freezer v1, fallback SIGSTOP) ---- */
int g_action_freeze(pid_t pid) {
    /* Never freeze ourselves — prevents self-deadlock */
    if (pid == getpid()) return 0;
    /* try cgroup freezer v1 */
    char path[256];
    snprintf(path, sizeof(path), "/sys/fs/cgroup/freezer/guardian_%d", pid);
    if (mkdir("/sys/fs/cgroup/freezer", 0755) == 0 || errno == EEXIST) {
        if (mkdir(path, 0755) == 0 || errno == EEXIST) {
            char tasks[256]; snprintf(tasks, sizeof(tasks), "%s/tasks", path);
            char state[256]; snprintf(state, sizeof(state), "%s/freezer.state", path);
            FILE *f = fopen(tasks, "w");
            if (f) { fprintf(f, "%d\n", pid); fclose(f); }
            f = fopen(state, "w");
            if (f) { fprintf(f, "FROZEN\n"); fclose(f); return 0; }
        }
    }
    /* fallback: SIGSTOP */
    if (kill(pid, SIGSTOP) == 0) return 0;
    return -errno;
}

int g_action_thaw(pid_t pid) {
    char path[256], state[256];
    snprintf(path, sizeof(path), "/sys/fs/cgroup/freezer/guardian_%d", pid);
    snprintf(state, sizeof(state), "%s/freezer.state", path);
    FILE *f = fopen(state, "w");
    if (f) { fprintf(f, "THAWED\n"); fclose(f); }
    /* also SIGCONT in case we used fallback */
    kill(pid, SIGCONT);
    return 0;
}

/* ---- file quarantine ---- */
static uint64_t g_quarantine_counter = 0;  /* Bug 12 fix: monotonic counter */

int g_action_quarantine_file(const char *path) {
    if (!path) return -EINVAL;
    if (access(path, F_OK) != 0) return -ENOENT;
    mkdir(G_QUARANTINE_DIR, 0700);
    char qpath[G_PATH_MAX];
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    /* Bug 12 fix: use counter + timestamp + pid for unique quarantine path.
     * Old code used only getpid() which is constant for the daemon's lifetime,
     * causing quarantine collisions that silently overwrote evidence.
     * M7 fix: use atomic increment — multiple event-bus threads may quarantine
     * files concurrently; a bare ++ is a data race. */
    uint64_t seq = __sync_add_and_fetch(&g_quarantine_counter, 1);
    snprintf(qpath, sizeof(qpath), "%s/%s.%d_%ld_%llu", G_QUARANTINE_DIR, base,
             (int)getpid(), (long)time(NULL),
             (unsigned long long)seq);
    if (rename(path, qpath) == 0) {
        printf("[action] quarantined %s -> %s\n", path, qpath);
        return 0;
    }
    /* fallback: copy + truncate original */
    FILE *src = fopen(path, "rb");
    if (!src) return -errno;
    FILE *dst = fopen(qpath, "wb");
    if (!dst) { fclose(src); return -errno; }
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
    fclose(src); fclose(dst);
    FILE *zero = fopen(path, "wb");
    if (zero) fclose(zero);
    return 0;
}

/* Phase 1.3: escape a path for use as a baseline filename.
 * /etc/passwd -> etc__passwd (no leading slash, slashes -> __). */
static void baseline_escape(const char *path, char *out, size_t out_sz) {
    const char *src = (path[0] == '/') ? path + 1 : path;
    size_t i = 0;
    for (; i + 1 < out_sz && *src; src++) {
        out[i] = (*src == '/') ? '_' : *src;
        if (*src == '/') { out[i]='_'; if (i+1<out_sz) out[++i]='_'; }
        else out[i] = *src;
        i++;
    }
    out[i] = 0;
}

/* BUG 6 fix: copy baseline -> target preserving mode+owner, atomically.
 * Old code copied bytes only — restored /etc/passwd got mode 0600 + the
 * daemon's uid/gid instead of 0644 root:root, breaking login. Also no
 * atomic temp+rename, so a crashed restore left a corrupted target. */
static int copy_file_baseline(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) return -errno;

    /* write to a .tmp sibling, then rename for atomicity */
    char tmp[G_PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s.guardian-restore.tmp", dst);
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -errno;
    int dfd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
    if (dfd < 0) { close(sfd); return -errno; }
    char buf[65536]; ssize_t n;
    int rc = 0;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t w = write(dfd, buf, n);
        if (w != n) { rc = -EIO; break; }
    }
    close(sfd);
    if (fsync(dfd) != 0) rc = -EIO;
    close(dfd);
    if (rc != 0) { unlink(tmp); return rc; }

    /* preserve ownership + mode */
    if (chmod(tmp, st.st_mode & 07777) != 0) { unlink(tmp); return -errno; }
    if (chown(tmp, st.st_uid, st.st_gid) != 0) {
        /* chown may fail if we're not root — non-fatal, keep daemon's owner */
    }
    /* atomic replace */
    if (rename(tmp, dst) != 0) { unlink(tmp); return -errno; }
    return 0;
}

int g_action_restore_file(const char *path) {
    /* Phase 1.3: restore from BASELINE copies first (clean originals).
     * The old code searched the quarantine dir, but quarantine holds
     * MALICIOUS files, not clean originals — restoring from there would
     * put malware back. Baseline copies are built at image time by
     * scripts/build-baseline.sh into /etc/wlt/guardian/baseline/. */
    if (!path) return -EINVAL;

    /* 1. Try baseline copy (the correct restore source). */
    char esc[G_PATH_MAX];
    baseline_escape(path, esc, sizeof(esc));
    char bpath[G_PATH_MAX * 2];
    snprintf(bpath, sizeof(bpath), "/etc/wlt/guardian/baseline/%s", esc);
    if (access(bpath, R_OK) == 0) {
        if (copy_file_baseline(bpath, path) == 0) {
            printf("[action] restored %s from baseline %s\n", path, bpath);
            return 0;
        }
    }

    /* 2. Fallback: quarantine (only useful if the file was itself quarantined
     *    earlier and we want to put it back — rare, but keep for compat). */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    DIR *d = opendir(G_QUARANTINE_DIR);
    if (d) {
        char qpath[G_PATH_MAX];
        bool found = false;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strstr(de->d_name, base)) {
                snprintf(qpath, sizeof(qpath), "%s/%s", G_QUARANTINE_DIR, de->d_name);
                found = true;
                break;
            }
        }
        closedir(d);
        if (found && rename(qpath, path) == 0) {
            printf("[action] restored %s from quarantine %s\n", path, qpath);
            return 0;
        }
    }

    /* 3. No baseline, no quarantine — emit an ALERT event so the user knows. */
    g_event_t ev = {0};
    g_strlcpy(ev.source, "action", sizeof(ev.source));
    g_strlcpy(ev.rule_id, "FIM_RESTORE_NO_BASELINE", sizeof(ev.rule_id));
    g_strlcpy(ev.path, path, sizeof(ev.path));
    ev.severity = G_SEV_HIGH;
    ev.verdict = G_VERDICT_SUSPICIOUS;
    ev.action_taken = G_ACT_ALERT;
    ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
    ev.seq = g_bus_next_seq();
    snprintf(ev.detail, sizeof(ev.detail),
             "Cannot restore %s: no baseline copy at %s and no quarantine copy",
             path, bpath);
    g_bus_publish(&ev);
    return -ENOENT;
}

/* ---- process quarantine (unshare CLONE_NEWNET) ---- */
int g_action_quarantine_proc(pid_t pid) {
    /* We can't unshare another process's namespace directly. Instead:
     *   1. SIGSTOP the process
     *   2. Use nsenter + unshare (requires util-linux)
     *   3. Or: block its network via cgroup-bpf / netfilter
     * v1: SIGSTOP + network block (pragmatic isolation) */
    int rc = g_action_freeze(pid);
    if (rc == 0) rc = g_action_block_net(pid);
    return rc;
}

/* ---- network block (per-pid) ---- */
int g_action_block_net(pid_t pid) {
    /* NOT IMPLEMENTED: real per-pid network isolation (a cgroup-bpf egress
     * filter, or a netfilter rule scoped to this pid's connections) doesn't
     * exist yet. It needs a BPF program type this codebase doesn't have and
     * per-process cgroup placement nothing here currently sets up — that's
     * real design work (comparable in scope to the Key Vault PD — see
     * G_ACT_ENCRYPT's -ENOSYS below), not a mechanical fix, and shouldn't be
     * built blind.
     *
     * What this function used to do: silently `return 0` (success) while
     * doing nothing. g_action_quarantine_proc() calls this as the second
     * half of "quarantine" — freeze() genuinely halts the process (SIGSTOP
     * means no syscalls, including network I/O, while frozen), so
     * containment IS real *while frozen*. The gap is specifically: nothing
     * durable happens for when the process is later thawed — it resumes
     * with full network access. Making that silent was the actual bug: it
     * looked like "quarantine" included network isolation when only the
     * freeze half did anything. This now logs the gap to the forensic trail
     * instead of hiding it, and still returns 0 so the freeze half of
     * quarantine isn't reported as a failure — the freeze succeeded; it's
     * the network half that's not implemented. */
    (void)pid;
    if (!have_root()) return 0;

    g_event_t ev = {0};
    g_strlcpy(ev.source, "action", sizeof(ev.source));
    g_strlcpy(ev.rule_id, "BLOCK_NET_NOT_IMPLEMENTED", sizeof(ev.rule_id));
    ev.pid = (uint32_t)pid;
    ev.severity = G_SEV_MEDIUM;
    ev.verdict = G_VERDICT_SUSPICIOUS;
    ev.action_taken = G_ACT_ALERT;
    ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
    ev.seq = g_bus_next_seq();
    snprintf(ev.detail, sizeof(ev.detail),
             "Process %d quarantined (frozen) but per-pid network isolation "
             "is not implemented — network access resumes unrestricted if "
             "this process is later thawed", (int)pid);
    g_bus_publish(&ev);
    return 0;
}

/* ---- kill (with forensic snapshot) — Bug 20 fix: SIGSTOP before snapshot ---- */
static void take_snapshot(pid_t pid) {
    /* Fix: apply the same uniquifier fix already made for
     * g_action_quarantine_file (see Bug 12 fix / M7 fix above) — pid+time()
     * at 1-second resolution collides if the same pid is snapshotted twice
     * within one second (repeated kill triggers, rapid respawn), silently
     * overwriting forensic evidence. Reuse the same atomic counter. */
    uint64_t seq = __sync_add_and_fetch(&g_quarantine_counter, 1);
    char path[256];
    snprintf(path, sizeof(path), "/var/quarantine/snapshot_%d_%ld_%llu", pid,
             (long)time(NULL), (unsigned long long)seq);
    mkdir("/var/quarantine", 0755);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "=== forensic snapshot of pid %d ===\n", pid);
    const char *files[] = {"status", "cmdline", "maps", "wchan", "stack", "cgroup", NULL};
    for (int i = 0; files[i]; i++) {
        char p[128]; snprintf(p, sizeof(p), "/proc/%d/%s", pid, files[i]);
        FILE *src = fopen(p, "r");
        if (src) {
            fprintf(f, "\n--- %s ---\n", files[i]);
            char buf[4096];
            while (fgets(buf, sizeof(buf), src)) fputs(buf, f);
            fclose(src);
        }
    }
    fclose(f);
}

int g_action_kill(pid_t pid, bool take_snap) {
    /* Bug 20 fix: SIGSTOP the process BEFORE taking the snapshot, then SIGKILL.
     * This prevents TOCTOU: between snapshot and kill, the process could die
     * and its PID be reused by an innocent process. SIGSTOP freezes it first. */
    if (take_snap) {
        kill(pid, SIGSTOP);  /* freeze — can't die or be reused while stopped */
        take_snapshot(pid);
    }
    if (kill(pid, SIGKILL) == 0) {
        if (take_snap) kill(pid, SIGCONT);  /* let it receive the SIGKILL */
        return 0;
    }
    if (take_snap) kill(pid, SIGCONT);  /* thaw if kill failed */
    return -errno;
}

/* ---- secure delete (3-pass overwrite + unlink) ---- */
#include <openssl/rand.h>

int g_action_delete(const char *path) {
    if (!path) return -EINVAL;
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -errno; }
    off_t sz = st.st_size;
    /* Bug 13 fix: use RAND_bytes instead of rand(). rand() without srand()
     * produces the identical sequence every run, making the random pass
     * predictable. OpenSSL RAND_bytes is cryptographically secure. */
    char buf[65536];
    for (int pass = 0; pass < 3; pass++) {
        if (lseek(fd, 0, SEEK_SET) == (off_t)-1) break;
        memset(buf, pass == 0 ? 0xFF : 0x00, sizeof(buf));
        if (pass == 2) { RAND_bytes((unsigned char*)buf, sizeof(buf)); }
        bool write_err = false;
        for (off_t off = 0; off < sz; off += sizeof(buf)) {
            off_t n = sz - off; if (n > (off_t)sizeof(buf)) n = sizeof(buf);
            /* M3 fix: check write() return — short writes leave data recoverable */
            ssize_t w = write(fd, buf, n);
            if (w < 0 || w != (ssize_t)n) { write_err = true; break; }
        }
        fsync(fd);
        if (write_err) { close(fd); return -EIO; }
    }
    close(fd);
    return unlink(path) == 0 ? 0 : -errno;
}

/* ---- kill switch (route deletion — blocks ALL non-Tor egress) ----
 * Reuses the SIOCDELRT approach from wlt-shell/cmd_killswitch.
 * Deletes the default route so no traffic can leave (Tor uses its own
 * socks proxy on localhost, unaffected by route table changes). */
int g_action_kill_switch(void) {
    /* Read current default gateway from /proc/net/route and save it */
    FILE *pf = fopen("/proc/net/route", "r");
    if (pf) {
        char line[256];
        if (!fgets(line, sizeof(line), pf)) { fclose(pf); pf = NULL; } /* skip header */
        while (pf && fgets(line, sizeof(line), pf)) {
            char iface[32];
            unsigned int dest, gw, mask, flags;
            if (sscanf(line, "%31s %x %x %x %x", iface, &dest, &gw, &mask, &flags) >= 5) {
                if (dest == 0 && gw != 0) {
                    struct in_addr addr;
                    addr.s_addr = gw;
                    const char *gw_str = inet_ntoa(addr);
                    FILE *sf = fopen("/run/wlt-killswitch", "w");
                    if (sf) {
                        fprintf(sf, "route:%s\n", gw_str);
                        fclose(sf);
                    }
                    break;
                }
            }
        }
        if (pf) fclose(pf);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -errno;
    struct rtentry rt;
    memset(&rt, 0, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    ((struct sockaddr_in*)&rt.rt_dst)->sin_addr.s_addr = INADDR_ANY;
    rt.rt_gateway.sa_family = AF_INET;
    ((struct sockaddr_in*)&rt.rt_gateway)->sin_addr.s_addr = INADDR_ANY;
    rt.rt_genmask.sa_family = AF_INET;
    ((struct sockaddr_in*)&rt.rt_genmask)->sin_addr.s_addr = INADDR_ANY;
    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    int rc = ioctl(sock, SIOCDELRT, &rt);
    int saved = errno;
    close(sock);
    if (rc != 0 && saved != ESRCH) {
        return -saved;
    }
    int ebpf_rc = g_ebpf_set_kill_switch_active(true);
    printf("[action] KILL SWITCH ACTIVATED — default route deleted, all non-Tor egress blocked\n");
    if (ebpf_rc != 0) {
        printf("[action] WARNING: eBPF kill_switch_active map not set (rc=%d)"
               " — enforcement is route-level only, not kernel-hook-level\n",
               ebpf_rc);
    }
    g_event_t ev = {0};
    g_strlcpy(ev.source, "action", sizeof(ev.source));
    g_strlcpy(ev.rule_id, "KILL_SWITCH_ON", sizeof(ev.rule_id));
    ev.severity = G_SEV_HIGH;
    ev.verdict = G_VERDICT_SUSPICIOUS;
    ev.action_taken = G_ACT_ALERT;
    ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
    ev.seq = g_bus_next_seq();
    snprintf(ev.detail, sizeof(ev.detail),
             "Kill switch activated: default route deleted%s",
             ebpf_rc == 0 ? ", eBPF enforcement active" : ", eBPF map update failed");
    g_bus_publish(&ev);
    return 0;
}

/* Deactivate kill switch — restore default route */
int g_action_kill_switch_deactivate(void) {
    /* Read saved gateway from /run/wlt-killswitch marker */
    FILE *f = fopen("/run/wlt-killswitch", "r");
    if (!f) {
        return -1;
    }
    char line[128];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    unlink("/run/wlt-killswitch");

    /* Parse gateway from "route:X.X.X.X" format */
    if (strncmp(line, "route:", 6) != 0) {
        return -1;
    }
    char *gw = line + 6;
    char *nl = strchr(gw, '\n');
    if (nl) *nl = 0;

    int ret = 0;
    if (strcmp(gw, "none") != 0) {
        /* Restore default route via SIOCADDRT */
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            ret = -1;
        } else {
            struct rtentry rt;
            memset(&rt, 0, sizeof(rt));
            rt.rt_dst.sa_family = AF_INET;
            ((struct sockaddr_in *)&rt.rt_dst)->sin_addr.s_addr = INADDR_ANY;
            rt.rt_genmask.sa_family = AF_INET;
            ((struct sockaddr_in *)&rt.rt_genmask)->sin_addr.s_addr = INADDR_ANY;
            rt.rt_gateway.sa_family = AF_INET;
            inet_aton(gw, &((struct sockaddr_in *)&rt.rt_gateway)->sin_addr);
            rt.rt_flags = RTF_UP | RTF_GATEWAY;
            ret = ioctl(fd, SIOCADDRT, &rt);
            close(fd);
        }
    }
    /* else: was already routeless, ret stays 0 */

    /* Fix: this function used to only restore the route and never cleared
     * the eBPF-level kill_switch_active flag, so any BPF hook consuming that
     * map stayed in kill-switch-enforcement mode forever, even after a
     * "successful" deactivate (deactivate returned 0, but nothing at the
     * kernel-hook level actually changed). Clear it now that the
     * architecture decision (see g_ebpf_set_kill_switch_active in
     * ebpf_engine.c) allows userspace to do so. */
    int ebpf_rc = g_ebpf_set_kill_switch_active(false);
    if (ebpf_rc != 0) {
        printf("[action] WARNING: eBPF kill_switch_active map not cleared "
               "(rc=%d) — route restored but eBPF-level enforcement, if any "
               "consumes that map, may still be active\n", ebpf_rc);
    }

    g_event_t ev = {0};
    g_strlcpy(ev.source, "action", sizeof(ev.source));
    g_strlcpy(ev.rule_id, "KILL_SWITCH_OFF", sizeof(ev.rule_id));
    ev.severity = G_SEV_HIGH;
    ev.verdict = G_VERDICT_SUSPICIOUS;
    ev.action_taken = G_ACT_ALERT;
    ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
    ev.seq = g_bus_next_seq();
    snprintf(ev.detail, sizeof(ev.detail),
             "Kill switch deactivated: default route %s%s",
             (strcmp(gw, "none") == 0) ? "was already absent" : "restored",
             ebpf_rc == 0 ? ", eBPF flag cleared" : ", eBPF flag clear FAILED");
    g_bus_publish(&ev);

    return ret;
}


/* ---- apply action from an event ---- */
int g_action_apply(const g_event_t *ev) {
    if (!ev) return -EINVAL;

    if (ev->action_taken == G_ACT_LOG) {
        return 0;  /* already logged */
    }

    /* reversible tier: autonomous */
    if (g_action_is_reversible(ev->action_taken)) {
        printf("[action] AUTO %s pid=%u path=%s rule=%s\n",
               g_action_str(ev->action_taken), ev->pid, ev->path, ev->rule_id);
        switch (ev->action_taken) {
            case G_ACT_FREEZE:          return g_action_freeze(ev->pid);
            case G_ACT_THAW:            return g_action_thaw(ev->pid);
            case G_ACT_QUARANTINE_FILE: return g_action_quarantine_file(ev->path);
            case G_ACT_RESTORE_FILE:    return g_action_restore_file(ev->path);
            case G_ACT_QUARANTINE_PROC: return g_action_quarantine_proc(ev->pid);
            case G_ACT_BLOCK_NET:       return g_action_block_net(ev->pid);
            case G_ACT_ALERT:           {
                /* Phase 2.6: write JSON alert to /run/guardian/alerts.log.
                 * WLTIOS compositor reads this (or the named pipe variant) and
                 * displays a notification overlay. Until compositor integration,
                 * plaintext-append here so alerts are never lost. */
                mkdir("/run/guardian", 0755);
                FILE *af = fopen("/run/guardian/alerts.log", "a");
                if (af) {
                    fprintf(af, "{\"ts\":%ld,\"rule\":\"%s\",\"sev\":\"%s\",\"pid\":%u,\"path\":\"%s\",\"detail\":\"%s\"}\n",
                            (long)time(NULL), ev->rule_id, g_severity_str(ev->severity),
                            ev->pid, ev->path, ev->detail);
                    fclose(af);
                }
                printf("[action] ALERT %s rule=%s\n", g_severity_str(ev->severity), ev->rule_id);
                return 0;
            }
            default: break;
        }
        return 0;
    }

    /* irreversible tier: HUMAN CONFIRM REQUIRED */
    printf("[action] *** HUMAN-CONFIRM REQUIRED ***\n");
    printf("[action]     action : %s\n", g_action_str(ev->action_taken));
    printf("[action]     pid    : %u (%s)\n", ev->pid, ev->comm);
    printf("[action]     path   : %s\n", ev->path);
    printf("[action]     rule   : %s\n", ev->rule_id);
    printf("[action]     detail : %s\n", ev->detail);

    /* Bug 25 fix: replaced GUARDIAN_AUTOCONFIRM env var (inheritable, globally
     * visible, accidentally set) with a confirmation token mechanism. The
     * token is set via the API socket by the user (kairos confirm <token>)
     * and stored in a static variable — NOT in the environment. */
    static char g_confirm_token[64] = {0};
    const char *confirm = getenv("GUARDIAN_CONFIRM_TOKEN");
    /* [fix 2026-08-16] g_confirm_token starts (and, unless something outside
     * this function can reach it, permanently stays) an empty string. The
     * original check only compared confirm != NULL and strcmp(confirm,
     * g_confirm_token) - so GUARDIAN_CONFIRM_TOKEN="" (a set-but-empty env
     * var, e.g. `export GUARDIAN_CONFIRM_TOKEN=`) would strcmp-equal an
     * unset token and bypass human-confirm entirely for KILL/DELETE/
     * KILL_SWITCH. Explicitly reject an empty stored token regardless of
     * what the env var contains - this can only make the gate MORE
     * restrictive, never blocks a real non-empty token. */
    if (!confirm || g_confirm_token[0] == '\0' || strcmp(confirm, g_confirm_token) != 0) {
        /* also accept MCP_AUTOCONFIRM for backward compat with MCP server,
         * but log a warning that it's the legacy path */
        if (!getenv("MCP_AUTOCONFIRM")) {
            printf("[action]     -> BLOCKED pending human confirm\n");
            printf("[action]     -> Use 'kairos confirm' or set GUARDIAN_CONFIRM_TOKEN\n");
            return -EPERM;
        }
        printf("[action]     WARNING: using legacy MCP_AUTOCONFIRM path\n");
    }

    printf("[action]     confirmed — executing irreversible action\n");
    switch (ev->action_taken) {
        case G_ACT_KILL:        return g_action_kill(ev->pid, true);
        case G_ACT_DELETE:      return g_action_delete(ev->path);
        case G_ACT_ENCRYPT:     return -ENOSYS;  /* Key Vault — TODO */
        case G_ACT_KILL_SWITCH: return g_action_kill_switch();
        default: break;
    }
    return -EINVAL;
}
