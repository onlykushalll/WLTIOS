/* SPDX-License-Identifier: GPL-2.0
 *
 * wlt-tor-mgr: Main Tor Control Manager
 *
 * Owns the long-running control connection to the Tor VM, polls status every
 * `status_interval` seconds, derives the WLT-Shell status indicator, tracks
 * bandwidth and drives the kill switch through the Firewall PD when Tor
 * becomes unreachable.
 *
 * The daemon entry (wlt_tor_mgr_run) is a blocking event loop; the CLI entry
 * (wlt_tor_cli_main) is a one-shot client used by `wlttor <cmd>`.
 *
 * Security: nothing identifying is logged. Identity keys live in the Key
 * Vault PD, never touched here.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "tor_mgr.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

/* ---- Logging ------------------------------------------------------------ */
static wlt_tor_log_level_t g_log_level = WLT_TOR_LOG_INFO;
static bool                 g_log_quiet = false;

void wlt_tor_log_set_level(wlt_tor_log_level_t lvl) { g_log_level = lvl; }
void wlt_tor_log_set_quiet(bool quiet)              { g_log_quiet = quiet; }

void wlt_tor_log(wlt_tor_log_level_t lvl, const char *fmt, ...)
{
    static const char *tag[] = { "DBG", "INF", "WRN", "ERR" };
    if (g_log_quiet || lvl < g_log_level)
        return;
    if (lvl < WLT_TOR_LOG_DEBUG || lvl > WLT_TOR_LOG_ERROR)
        return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    fprintf(stderr, "%s %04d-%02d-%02d %02d:%02d:%02d ",
            tag[lvl],
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ---- Kill switch (delegates to Firewall PD in production) --------------- *
 * Here we keep process-local state so the rest of the OS has a consistent
 * answer. A real build replaces these bodies with an seL4 IPC message to the
 * Firewall PD, which enforces the block at the network layer.
 */
static wlt_tor_killswitch_t g_ks = WLT_TOR_KS_NORMAL;

int wlt_tor_kill_switch_trigger(void)
{
    if (g_ks != WLT_TOR_KS_TRIGGERED) {
        g_ks = WLT_TOR_KS_TRIGGERED;
        wlt_log_wrn("KILL SWITCH triggered - Firewall PD blocking all traffic");
    }
    return 0;
}

int wlt_tor_kill_switch_release(void)
{
    if (g_ks != WLT_TOR_KS_NORMAL) {
        wlt_log_inf("kill switch released - traffic resuming");
    }
    g_ks = WLT_TOR_KS_NORMAL;
    return 0;
}

wlt_tor_killswitch_t wlt_tor_kill_switch_state(void) { return g_ks; }

/* ---- Manager ------------------------------------------------------------ */
struct wlt_tor_mgr {
    wlt_tor_config_t        cfg;
    wlt_tor_transport_t    *tp;
    wlt_tor_control_t       ctl;
    bool                    connected;
    bool                    running;
    volatile sig_atomic_t   stop_req;
    pthread_mutex_t         lock;
    wlt_tor_status_snapshot_t snap;
    /* bandwidth rate tracking */
    uint64_t prev_read;
    uint64_t prev_written;
    time_t   prev_bw_ts;
    bool     have_prev_bw;
};

const wlt_tor_config_t *wlt_tor_mgr_config(wlt_tor_mgr_t *m)
{
    return m ? &m->cfg : NULL;
}

static int mgr_connect_locked(wlt_tor_mgr_t *m)
{
    int rc;
    if (m->connected)
        return 0;
    if (!m->tp) {
        m->tp = wlt_tor_transport_tcp();
        if (!m->tp)
            return -ENOMEM;
    }
    rc = wlt_tor_control_open(&m->ctl, m->tp, m->cfg.control_host,
                              m->cfg.control_port);
    if (rc < 0) {
        wlt_log_dbg("control open failed: %s", strerror(-rc));
        return rc;
    }
    /* Prefer cookie auth; fall back to null auth for dev/test tor instances. */
    if (wlt_tor_control_authenticate_cookie(&m->ctl, m->cfg.cookie_path) < 0)
        wlt_tor_control_authenticate_null(&m->ctl);
    m->connected = true;
    return 0;
}

static void mgr_disconnect_locked(wlt_tor_mgr_t *m)
{
    if (m->connected) {
        wlt_tor_control_close(&m->ctl);
        m->connected = false;
    }
}

static uint32_t parse_bootstrap(const char *s)
{
    const char *p = strstr(s, "PROGRESS=");
    if (!p)
        return 0;
    return (uint32_t)strtoul(p + strlen("PROGRESS="), NULL, 10);
}

static int parse_traffic(const char *s, uint64_t *rd, uint64_t *wr)
{
    /* "traffic/readwrite=<read>,<written>" */
    const char *p = strchr(s, '=');
    if (!p)
        return -1;
    p++;
    char *end;
    unsigned long long r = strtoull(p, &end, 10);
    if (end == p || *end != ',')
        return -1;
    unsigned long long w = strtoull(end + 1, NULL, 10);
    *rd = r;
    *wr = w;
    return 0;
}

static void poll_status_locked(wlt_tor_mgr_t *m)
{
    char buf[512];

    if (mgr_connect_locked(m) < 0) {
        m->snap.status = WLT_TOR_STATUS_DOWN;
        m->snap.bootstrap_pct = 0;
        m->snap.last_status_at = time(NULL);
        /* Tor VM unreachable -> kill switch per spec error-handling table. */
        wlt_tor_kill_switch_trigger();
        mgr_disconnect_locked(m);
        return;
    }

    /* Version (cheap; refresh occasionally). */
    if (!m->snap.tor_version[0]) {
        if (wlt_tor_control_getinfo(&m->ctl, "version",
                                    m->snap.tor_version,
                                    sizeof(m->snap.tor_version)) == 0) {
            /* truncate already handled by getinfo bounds */
        }
    }

    uint32_t pct = 0;
    if (wlt_tor_control_getinfo(&m->ctl, "status/bootstrap-phase", buf, sizeof(buf)) == 0)
        pct = parse_bootstrap(buf);
    m->snap.bootstrap_pct = pct;

    uint64_t rd = 0, wr = 0;
    if (wlt_tor_control_getinfo(&m->ctl, "traffic/readwrite", buf, sizeof(buf)) == 0)
        parse_traffic(buf, &rd, &wr);

    time_t now = time(NULL);
    m->snap.bandwidth.bytes_read = rd;
    m->snap.bandwidth.bytes_written = wr;
    if (m->have_prev_bw) {
        time_t dt = now - m->prev_bw_ts;
        if (dt <= 0)
            dt = 1;
        m->snap.bandwidth.rate_bps_down = (rd - m->prev_read) / (uint64_t)dt;
        m->snap.bandwidth.rate_bps_up   = (wr - m->prev_written) / (uint64_t)dt;
    } else {
        m->snap.bandwidth.rate_bps_down = 0;
        m->snap.bandwidth.rate_bps_up   = 0;
    }
    m->prev_read = rd;
    m->prev_written = wr;
    m->prev_bw_ts = now;
    m->have_prev_bw = true;
    m->snap.bandwidth.sampled_at = now;

    /* Derive aggregate status. */
    wlt_tor_status_t prev = m->snap.status;
    if (pct >= 100)
        m->snap.status = WLT_TOR_STATUS_ACTIVE;
    else
        m->snap.status = WLT_TOR_STATUS_CONNECTING;

    snprintf(m->snap.socks_addr, sizeof(m->snap.socks_addr), "%s:%u",
             m->cfg.control_host, (unsigned)m->cfg.socks_port);

    m->snap.last_status_at = now;
    m->snap.kill_switch = wlt_tor_kill_switch_state();

    /* Transition DOWN/triggered -> ACTIVE releases the kill switch. */
    if (prev == WLT_TOR_STATUS_DOWN && m->snap.status != WLT_TOR_STATUS_DOWN)
        wlt_tor_kill_switch_release();

    if (prev != m->snap.status)
        wlt_log_inf("status: %s -> %s (bootstrap=%u%%)",
                    wlt_tor_status_to_str(prev),
                    wlt_tor_status_to_str(m->snap.status), pct);
}

int wlt_tor_mgr_init(wlt_tor_mgr_t **mgr, const wlt_tor_config_t *cfg)
{
    wlt_tor_mgr_t *m;
    if (!mgr)
        return -EINVAL;
    m = calloc(1, sizeof(*m));
    if (!m)
        return -ENOMEM;
    if (cfg)
        m->cfg = *cfg;
    else
        wlt_tor_config_init_defaults(&m->cfg);
    if (m->cfg.status_interval == 0)
        m->cfg.status_interval = WLT_TOR_DEFAULT_INTERVAL;
    pthread_mutex_init(&m->lock, NULL);
    m->snap.status = WLT_TOR_STATUS_UNKNOWN;
    snprintf(m->snap.exit_country, sizeof(m->snap.exit_country), "%s",
             m->cfg.exit_mode == WLT_TOR_EXIT_COUNTRY ? m->cfg.exit_country : "");
    *mgr = m;
    wlt_log_inf("manager initialised (control=%s:%u interval=%us)",
                m->cfg.control_host, (unsigned)m->cfg.control_port,
                (unsigned)m->cfg.status_interval);
    return 0;
}

void wlt_tor_mgr_shutdown(wlt_tor_mgr_t *m)
{
    if (!m)
        return;
    pthread_mutex_lock(&m->lock);
    mgr_disconnect_locked(m);
    pthread_mutex_unlock(&m->lock);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

int wlt_tor_mgr_stop(wlt_tor_mgr_t *m)
{
    if (!m)
        return -EINVAL;
    m->stop_req = 1;
    return 0;
}

/* Sleep for `sec` seconds but wake every 100ms to honour stop_req. */
static void interruptible_sleep(wlt_tor_mgr_t *m, unsigned sec)
{
    for (unsigned i = 0; i < sec * 10 && !m->stop_req; i++) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
}

int wlt_tor_mgr_run(wlt_tor_mgr_t *m)
{
    if (!m)
        return -EINVAL;
    m->running = true;
    m->stop_req = 0;

    /* Initial poll so the tray shows something quickly. */
    pthread_mutex_lock(&m->lock);
    poll_status_locked(m);
    pthread_mutex_unlock(&m->lock);

    while (!m->stop_req) {
        pthread_mutex_lock(&m->lock);
        poll_status_locked(m);
        /* Opportunistically count circuits so the snapshot stays fresh. */
        if (m->connected && m->snap.status == WLT_TOR_STATUS_ACTIVE) {
            wlt_tor_circuit_t circs[WLT_TOR_MAX_CIRCUITS];
            size_t n = 0;
            if (wlt_tor_circuits_refresh(&m->ctl, circs, WLT_TOR_MAX_CIRCUITS, &n) == 0)
                m->snap.circuit_count = n;
        }
        pthread_mutex_unlock(&m->lock);
        interruptible_sleep(m, m->cfg.status_interval ? m->cfg.status_interval : 1);
    }
    m->running = false;
    return 0;
}

int wlt_tor_mgr_get_status(wlt_tor_mgr_t *m, wlt_tor_status_snapshot_t *out)
{
    if (!m || !out)
        return -EINVAL;
    pthread_mutex_lock(&m->lock);
    *out = m->snap;
    pthread_mutex_unlock(&m->lock);
    return 0;
}

int wlt_tor_mgr_get_circuits(wlt_tor_mgr_t *m, wlt_tor_circuit_t *out,
                             size_t max, size_t *count)
{
    int rc;
    if (!m || !out || !count)
        return -EINVAL;
    pthread_mutex_lock(&m->lock);
    if (mgr_connect_locked(m) < 0) {
        *count = 0;
        rc = -ENOTCONN;
    } else {
        rc = wlt_tor_circuits_refresh(&m->ctl, out, max, count);
    }
    pthread_mutex_unlock(&m->lock);
    return rc;
}

int wlt_tor_mgr_new_identity(wlt_tor_mgr_t *m)
{
    int rc;
    if (!m)
        return -EINVAL;
    pthread_mutex_lock(&m->lock);
    if (mgr_connect_locked(m) < 0)
        rc = -ENOTCONN;
    else
        rc = wlt_tor_new_identity(&m->ctl);
    if (rc == 0)
        m->snap.last_identity_at = time(NULL);
    pthread_mutex_unlock(&m->lock);
    return rc;
}

int wlt_tor_mgr_set_exit(wlt_tor_mgr_t *m, const char *country)
{
    int rc;
    if (!m)
        return -EINVAL;
    pthread_mutex_lock(&m->lock);
    if (mgr_connect_locked(m) < 0) {
        rc = -ENOTCONN;
        goto out;
    }
    if (country && *country) {
        char val[8];
        snprintf(val, sizeof(val), "{%s}", country);
        rc  = wlt_tor_control_setconf(&m->ctl, "ExitNodes", val);
        rc |= wlt_tor_control_setconf(&m->ctl, "StrictNodes", "1");
        if (rc == 0) {
            m->cfg.exit_mode = WLT_TOR_EXIT_COUNTRY;
            snprintf(m->cfg.exit_country, sizeof(m->cfg.exit_country), "%s", country);
            snprintf(m->snap.exit_country, sizeof(m->snap.exit_country), "%s", country);
        }
    } else {
        rc  = wlt_tor_control_resetconf(&m->ctl, "ExitNodes");
        rc |= wlt_tor_control_setconf(&m->ctl, "StrictNodes", "0");
        if (rc == 0) {
            m->cfg.exit_mode = WLT_TOR_EXIT_AUTO;
            m->snap.exit_country[0] = '\0';
        }
    }
out:
    pthread_mutex_unlock(&m->lock);
    return rc;
}

int wlt_tor_mgr_get_bandwidth(wlt_tor_mgr_t *m, wlt_tor_bandwidth_t *out)
{
    if (!m || !out)
        return -EINVAL;
    pthread_mutex_lock(&m->lock);
    *out = m->snap.bandwidth;
    pthread_mutex_unlock(&m->lock);
    return 0;
}

/* ---- CLI ---------------------------------------------------------------- */
static volatile sig_atomic_t g_cli_stop;
static wlt_tor_mgr_t        *g_daemon_mgr;

static void cli_sighandler(int sig)
{
    (void)sig;
    g_cli_stop = 1;
    if (g_daemon_mgr)
        wlt_tor_mgr_stop(g_daemon_mgr);
}

static int cli_open_control(const wlt_tor_config_t *cfg, wlt_tor_control_t *ctl)
{
    wlt_tor_transport_t *tp;
    int rc;
    tp = wlt_tor_transport_tcp();
    if (!tp)
        return -ENOMEM;
    rc = wlt_tor_control_open(ctl, tp, cfg->control_host, cfg->control_port);
    if (rc < 0)
        return rc;
    if (wlt_tor_control_authenticate_cookie(ctl, cfg->cookie_path) < 0)
        wlt_tor_control_authenticate_null(ctl);
    return 0;
}

static int cmd_status(const wlt_tor_config_t *cfg)
{
    wlt_tor_control_t ctl;
    char buf[256];
    if (cli_open_control(cfg, &ctl) < 0) {
        printf("Tor: DOWN (control port unreachable)\n");
        return 1;
    }
    uint32_t pct = 0;
    if (wlt_tor_control_getinfo(&ctl, "status/bootstrap-phase", buf, sizeof(buf)) == 0)
        pct = parse_bootstrap(buf);
    const char *st = (pct >= 100) ? "ACTIVE" : (pct > 0 ? "CONNECTING" : "DOWN");
    printf("Tor: %s (bootstrap %u%%)\n", st, pct);
    if (wlt_tor_control_getinfo(&ctl, "version", buf, sizeof(buf)) == 0)
        printf("version: %s\n", buf);
    if (wlt_tor_control_getinfo(&ctl, "traffic/readwrite", buf, sizeof(buf)) == 0) {
        uint64_t rd = 0, wr = 0;
        parse_traffic(buf, &rd, &wr);
        printf("traffic: rx=%llu tx=%llu\n",
               (unsigned long long)rd, (unsigned long long)wr);
    }
    printf("kill-switch: %s\n", wlt_tor_killswitch_to_str(wlt_tor_kill_switch_state()));
    wlt_tor_control_close(&ctl);
    return 0;
}

static const char *role_of(size_t idx, size_t count)
{
    if (count == 0)
        return "?";
    if (idx == 0)
        return "entry";
    if (idx == count - 1)
        return "exit";
    return "middle";
}

static int cmd_circuits(const wlt_tor_config_t *cfg)
{
    wlt_tor_control_t ctl;
    wlt_tor_circuit_t circs[WLT_TOR_MAX_CIRCUITS];
    size_t n = 0;
    if (cli_open_control(cfg, &ctl) < 0) {
        printf("Tor: DOWN\n");
        return 1;
    }
    if (wlt_tor_circuits_refresh(&ctl, circs, WLT_TOR_MAX_CIRCUITS, &n) < 0) {
        printf("no circuits available\n");
        wlt_tor_control_close(&ctl);
        return 1;
    }
    printf("%zu circuit(s):\n", n);
    for (size_t i = 0; i < n; i++) {
        printf("  [%u] %s purpose=%s nodes=%zu\n",
               circs[i].id, circs[i].status, circs[i].purpose, circs[i].node_count);
        char fbuf[128];
        for (size_t j = 0; j < circs[i].node_count; j++) {
            wlt_tor_node_t *nd = &circs[i].nodes[j];
            printf("      %s  %-20s %-12s %-6s [%s]\n", role_of(j, circs[i].node_count),
                   nd->nickname[0] ? nd->nickname : "?",
                   nd->ipv4[0] ? nd->ipv4 : "?",
                   nd->country[0] ? nd->country : "??",
                   wlt_tor_node_flag_str(nd->flags, fbuf, sizeof(fbuf)));
        }
    }
    wlt_tor_control_close(&ctl);
    return 0;
}

static int cmd_new_identity(const wlt_tor_config_t *cfg)
{
    wlt_tor_control_t ctl;
    if (cli_open_control(cfg, &ctl) < 0) {
        printf("Tor: DOWN - cannot rotate identity\n");
        return 1;
    }
    int rc = wlt_tor_new_identity(&ctl);
    wlt_tor_control_close(&ctl);
    printf(rc == 0 ? "new identity requested\n"
                   : "new identity failed\n");
    return rc == 0 ? 0 : 1;
}

static int cmd_set_exit(const wlt_tor_config_t *cfg, const char *country)
{
    wlt_tor_control_t ctl;
    if (cli_open_control(cfg, &ctl) < 0) {
        printf("Tor: DOWN\n");
        return 1;
    }
    int rc;
    if (country && strcasecmp(country, "auto") == 0) {
        rc  = wlt_tor_control_resetconf(&ctl, "ExitNodes");
        rc |= wlt_tor_control_setconf(&ctl, "StrictNodes", "0");
        printf(rc ? "failed to set auto exit\n" : "exit selection: auto\n");
    } else if (country && *country) {
        char val[8];
        snprintf(val, sizeof(val), "{%s}", country);
        rc  = wlt_tor_control_setconf(&ctl, "ExitNodes", val);
        rc |= wlt_tor_control_setconf(&ctl, "StrictNodes", "1");
        printf(rc ? "failed to set exit country\n" : "exit selection: %s\n", country);
    } else {
        printf("usage: wlttor set-exit <country|auto>\n");
        rc = -1;
    }
    wlt_tor_control_close(&ctl);
    return rc == 0 ? 0 : 1;
}

static int cmd_add_bridge(wlt_tor_config_t *cfg, int argc, char **argv)
{
    wlt_tor_bridge_t br;
    wlt_tor_control_t ctl;
    char spec[WLT_TOR_MAX_LINE];
    size_t off = 0;

    if (argc < 1) {
        printf("usage: wlttor add-bridge <[pt] host:port [fp] [args]>\n");
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        int w = snprintf(spec + off, sizeof(spec) - off, "%s%s",
                         i ? " " : "", argv[i]);
        if (w < 0 || (size_t)w >= sizeof(spec) - off) {
            printf("bridge spec too long\n");
            return 1;
        }
        off += (size_t)w;
    }
    if (wlt_tor_bridge_parse(spec, &br) != 0) {
        printf("invalid bridge spec: %s\n", spec);
        return 1;
    }
    int rc = wlt_tor_bridges_add(cfg, &br);
    if (rc == -EEXIST) {
        printf("bridge already configured: %s\n", br.addr);
        return 0;
    }
    if (rc < 0) {
        printf("cannot add bridge: %s\n", strerror(-rc));
        return 1;
    }
    if (cli_open_control(cfg, &ctl) < 0) {
        printf("Tor: DOWN - bridge saved but not applied\n");
    } else {
        wlt_tor_bridges_apply(&ctl, cfg);
        wlt_tor_control_close(&ctl);
        printf("bridge added and applied: %s\n", br.addr);
    }
    wlt_tor_config_save(cfg, NULL);
    return 0;
}

static int cmd_bandwidth(const wlt_tor_config_t *cfg)
{
    wlt_tor_control_t ctl;
    char buf[256];
    if (cli_open_control(cfg, &ctl) < 0) {
        printf("Tor: DOWN\n");
        return 1;
    }
    if (wlt_tor_control_getinfo(&ctl, "traffic/readwrite", buf, sizeof(buf)) < 0) {
        printf("bandwidth unavailable\n");
        wlt_tor_control_close(&ctl);
        return 1;
    }
    uint64_t rd = 0, wr = 0;
    parse_traffic(buf, &rd, &wr);
    printf("cumulative: rx=%llu B tx=%llu B\n",
           (unsigned long long)rd, (unsigned long long)wr);
    wlt_tor_control_close(&ctl);
    return 0;
}

static int cmd_daemon(wlt_tor_config_t *cfg)
{
    wlt_tor_mgr_t *m = NULL;
    int rc;
    struct sigaction sa = { .sa_handler = cli_sighandler };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    rc = wlt_tor_mgr_init(&m, cfg);
    if (rc < 0) {
        printf("init failed: %s\n", strerror(-rc));
        return 1;
    }
    wlt_log_inf("wlt-tor-mgr daemon starting");
    /* wlt_tor_mgr_run blocks until stop_req is set; the signal handler flips
     * it via g_daemon_mgr so SIGINT/SIGTERM produce a clean shutdown. */
    g_daemon_mgr = m;
    wlt_tor_mgr_run(m);
    g_daemon_mgr = NULL;
    wlt_tor_mgr_shutdown(m);
    wlt_log_inf("wlt-tor-mgr daemon stopped");
    return 0;
}

static void usage(const char *prog)
{
    printf("WLTIOS Tor Control Manager v%s\n\n"
           "usage: %s <command> [args]\n\n"
           "commands:\n"
           "  status                 show Tor status\n"
           "  circuits               list current circuits\n"
           "  new-identity           rotate Tor identity (NEWNYM + cookie clear)\n"
           "  set-exit <country|auto> set exit country (ISO code) or auto\n"
           "  add-bridge <spec>      add a Tor bridge and apply it\n"
           "  kill-switch            manually trigger the network kill switch\n"
           "  bandwidth              show cumulative Tor bandwidth\n"
           "  daemon                 run the status-monitoring daemon\n"
           "  help                   show this help\n",
           WLT_TOR_MGR_VERSION, prog);
}

int wlt_tor_cli_main(int argc, char **argv)
{
    wlt_tor_config_t cfg;
    const char *cmd;

    wlt_tor_config_init_defaults(&cfg);
    wlt_tor_config_load(&cfg, NULL);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    cmd = argv[1];

    if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
        usage(argv[0]);
        return 0;
    }
    if (!strcmp(cmd, "status"))
        return cmd_status(&cfg);
    if (!strcmp(cmd, "circuits"))
        return cmd_circuits(&cfg);
    if (!strcmp(cmd, "new-identity"))
        return cmd_new_identity(&cfg);
    if (!strcmp(cmd, "set-exit")) {
        if (argc < 3) {
            printf("usage: wlttor set-exit <country|auto>\n");
            return 1;
        }
        return cmd_set_exit(&cfg, argv[2]);
    }
    if (!strcmp(cmd, "add-bridge"))
        return cmd_add_bridge(&cfg, argc - 2, &argv[2]);
    if (!strcmp(cmd, "bandwidth"))
        return cmd_bandwidth(&cfg);
    if (!strcmp(cmd, "kill-switch")) {
        wlt_tor_kill_switch_trigger();
        printf("kill switch triggered\n");
        return 0;
    }
    if (!strcmp(cmd, "daemon"))
        return cmd_daemon(&cfg);

    printf("unknown command: %s\n", cmd);
    usage(argv[0]);
    return 1;
}

/* Process entry: forward to the CLI dispatcher. Keeping main() trivial lets
 * the same object set be linked into both the `wlttor` binary and a larger
 * WLTIOS composite that calls wlt_tor_cli_main() directly. */
#ifndef WLT_TESTING
int main(int argc, char **argv)
{
    return wlt_tor_cli_main(argc, argv);
}

#endif /* WLT_TESTING */
