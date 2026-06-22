/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wlt-tor-mgr: Tor Control Manager - Public API
 *
 * WLTIOS Tor Control Manager. Talks to the Tor daemon running in the second
 * Linux guest VM (the "Tor VM") over seL4 IPC, routed through the Firewall PD.
 * This module owns: status reporting, circuit viewing, identity rotation,
 * bridge configuration, bandwidth monitoring and kill-switch integration.
 *
 * Per ADR-03, Tor never runs natively in seL4. WLT-Tor-Mgr runs in the main
 * Linux guest and only ever sees the Tor *control* protocol; it has no direct
 * access to the Tor daemon socket. Identity keys live in the Key Vault PD.
 *
 * See: docs/specs/wlt-tor-mgr.md
 */
#ifndef WLT_TOR_MGR_H
#define WLT_TOR_MGR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Module identity ---------------------------------------------------- */
#define WLT_TOR_MGR_VERSION  "1.0.0"
#define WLT_TOR_MGR_NAME     "wlt-tor-mgr"

/* ---- Limits ------------------------------------------------------------- */
#define WLT_TOR_MAX_CIRCUITS      64
#define WLT_TOR_MAX_NODES         8    /* relays per circuit path            */
#define WLT_TOR_MAX_BRIDGES       32
#define WLT_TOR_MAX_LINE          1024
#define WLT_TOR_MAX_REPLY         (64 * 1024)
#define WLT_TOR_MAX_COUNTRY       4    /* ISO 3166-1 alpha-2 + NUL           */
#define WLT_TOR_MAX_NICKNAME      32
#define WLT_TOR_MAX_FINGERPRINT   64    /* "$" + 40 hex + NUL                 */
#define WLT_TOR_MAX_ADDR          128
#define WLT_TOR_MAX_PT_ARGS       256
#define WLT_TOR_MAX_HOSTPORT      (WLT_TOR_MAX_ADDR + 16) /* "host:port" slack */
#define WLT_TOR_CONFIG_PATH       "/etc/wlt/tor.conf"
#define WLT_TOR_COOKIE_PATH       "/var/lib/tor/control_auth_cookie"
#define WLT_TOR_COOKIE_JAR        "/var/lib/wlt/cookie-jar"
#define WLT_TOR_DEFAULT_INTERVAL  1     /* seconds between status polls       */
#define WLT_TOR_NEWIDENTITY_MS    5000  /* < 5s budget (spec)                 */

/* ---- Tor daemon status -------------------------------------------------- */
typedef enum {
    WLT_TOR_STATUS_UNKNOWN = 0,
    WLT_TOR_STATUS_DOWN,        /* Tor VM unreachable / daemon dead         */
    WLT_TOR_STATUS_CONNECTING,  /* bootstrapping / reconnecting             */
    WLT_TOR_STATUS_ACTIVE       /* circuits built, SOCKS ready              */
} wlt_tor_status_t;

/* Kill switch states (enforced by the Firewall PD) */
typedef enum {
    WLT_TOR_KS_NORMAL = 0,
    WLT_TOR_KS_TRIGGERED,   /* Firewall PD is blocking ALL traffic          */
    WLT_TOR_KS_RECOVERING   /* Tor VM restarting, traffic still blocked     */
} wlt_tor_killswitch_t;

/* ---- Relay / circuit model --------------------------------------------- */
/* Node flags - a subset of the Tor consensus "flags" field. */
#define WLT_TOR_NODE_GUARD      0x0001
#define WLT_TOR_NODE_EXIT       0x0002
#define WLT_TOR_NODE_MIDDLE     0x0004
#define WLT_TOR_NODE_AUTHORITY  0x0008
#define WLT_TOR_NODE_FAST       0x0010
#define WLT_TOR_NODE_STABLE     0x0020
#define WLT_TOR_NODE_RUNNING    0x0040
#define WLT_TOR_NODE_V2DIR      0x0080
#define WLT_TOR_NODE_HSDIR      0x0100

typedef struct {
    char     nickname[WLT_TOR_MAX_NICKNAME];
    char     fingerprint[WLT_TOR_MAX_FINGERPRINT];
    char     ipv4[WLT_TOR_MAX_ADDR];
    char     country[WLT_TOR_MAX_COUNTRY]; /* ISO 3166-1 alpha-2           */
    uint32_t flags;
    uint32_t latency_ms;   /* best-effort RTT; 0 if unavailable              */
} wlt_tor_node_t;

typedef struct {
    uint32_t        id;                          /* circuit id                */
    char            status[16];                  /* BUILT/EXTENDED/.../CLOSED */
    char            purpose[32];                 /* GENERAL, HS_CLIENT, ...   */
    wlt_tor_node_t  nodes[WLT_TOR_MAX_NODES];
    size_t          node_count;
    time_t          built_at;
    uint64_t        bytes_read;                  /* bytes via this circuit    */
    uint64_t        bytes_written;
} wlt_tor_circuit_t;

/* ---- Bridges ------------------------------------------------------------ */
typedef struct {
    char addr[WLT_TOR_MAX_ADDR];        /* host:port                          */
    char fingerprint[WLT_TOR_MAX_FINGERPRINT];
    char pt_type[32];                   /* "" | obfs4 | meek_lite | snowflake */
    char pt_args[WLT_TOR_MAX_PT_ARGS];  /* transport arguments                */
    bool enabled;
} wlt_tor_bridge_t;

/* ---- Bandwidth ---------------------------------------------------------- */
typedef struct {
    uint64_t bytes_read;     /* cumulative bytes pulled from Tor            */
    uint64_t bytes_written;  /* cumulative bytes pushed to Tor              */
    uint64_t rate_bps_down;  /* instantaneous down rate (B/s)               */
    uint64_t rate_bps_up;    /* instantaneous up rate (B/s)                 */
    time_t   sampled_at;
} wlt_tor_bandwidth_t;

/* ---- Exit selection ----------------------------------------------------- */
typedef enum {
    WLT_TOR_EXIT_AUTO = 0,
    WLT_TOR_EXIT_COUNTRY
} wlt_tor_exit_mode_t;

/* ---- Config ------------------------------------------------------------- */
typedef struct {
    /* [wlt-tor-mgr] */
    uint16_t socks_port;
    uint16_t dns_port;
    uint16_t control_port;
    char     control_host[WLT_TOR_MAX_ADDR];
    uint32_t status_interval;            /* seconds between status polls     */
    /* [bridges] */
    bool             bridges_enabled;
    wlt_tor_bridge_t bridges[WLT_TOR_MAX_BRIDGES];
    size_t           bridge_count;
    /* [exit] */
    wlt_tor_exit_mode_t exit_mode;
    char                exit_country[WLT_TOR_MAX_COUNTRY];
    /* runtime paths */
    char config_path[256];
    char cookie_path[256];
    char cookie_jar[256];
} wlt_tor_config_t;

/* ---- Status snapshot (what WLT-Shell / WLT-Settings see) --------------- */
typedef struct {
    wlt_tor_status_t       status;
    wlt_tor_killswitch_t   kill_switch;
    char                   tor_version[32];
    char                   socks_addr[WLT_TOR_MAX_HOSTPORT];
    uint32_t               bootstrap_pct;  /* 0..100                       */
    size_t                 circuit_count;
    wlt_tor_bandwidth_t    bandwidth;
    time_t                 last_status_at;
    time_t                 last_identity_at;
    char                   exit_country[WLT_TOR_MAX_COUNTRY];
} wlt_tor_status_snapshot_t;

/* ---- Transport abstraction --------------------------------------------- *
 * In production this is seL4 IPC to the Tor VM via the Firewall PD.  A TCP
 * socket implementation is provided so the Tor *control* protocol can be
 * exercised against a real tor --ControlPort during development.  Both share
 * the identical line-oriented protocol defined in control-spec.txt.
 */
typedef struct wlt_tor_transport wlt_tor_transport_t;
struct wlt_tor_transport {
    int     (*connect)(wlt_tor_transport_t *self, const char *host, uint16_t port);
    ssize_t (*send)(wlt_tor_transport_t *self, const void *buf, size_t len);
    ssize_t (*recv)(wlt_tor_transport_t *self, void *buf, size_t len);
    int     (*close)(wlt_tor_transport_t *self);
    void    *ctx;
};

/* Returns a singleton TCP transport. NULL on allocation failure. */
wlt_tor_transport_t *wlt_tor_transport_tcp(void);

/* ---- Control protocol (control.c) -------------------------------------- */
typedef struct {
    wlt_tor_transport_t *tp;
    bool                 authenticated;
    bool                 owns_transport;   /* close on control_close()        */
    char                 server_version[64];
} wlt_tor_control_t;

int  wlt_tor_control_open(wlt_tor_control_t *ctl, wlt_tor_transport_t *tp,
                          const char *host, uint16_t port);
int  wlt_tor_control_authenticate_cookie(wlt_tor_control_t *ctl,
                                         const char *cookie_path);
int  wlt_tor_control_authenticate_null(wlt_tor_control_t *ctl);
int  wlt_tor_control_getinfo(wlt_tor_control_t *ctl, const char *key,
                             char *out, size_t out_len);
int  wlt_tor_control_setconf(wlt_tor_control_t *ctl, const char *key,
                             const char *value);
int  wlt_tor_control_resetconf(wlt_tor_control_t *ctl, const char *key);
int  wlt_tor_control_signal(wlt_tor_control_t *ctl, const char *sig);
int  wlt_tor_control_close(wlt_tor_control_t *ctl);

/* Raw line command. Returns the 3-digit reply code; reply buffer holds the
 * assembled data payload (continuation '+' lines joined with '\n'). */
int  wlt_tor_control_command(wlt_tor_control_t *ctl, const char *cmd,
                             char *reply, size_t reply_len);

/* ---- Config (config.c) -------------------------------------------------- */
int  wlt_tor_config_init_defaults(wlt_tor_config_t *cfg);
int  wlt_tor_config_load(wlt_tor_config_t *cfg, const char *path);
int  wlt_tor_config_save(const wlt_tor_config_t *cfg, const char *path);

/* ---- Circuits (circuits.c) --------------------------------------------- */
int  wlt_tor_circuits_refresh(wlt_tor_control_t *ctl, wlt_tor_circuit_t *out,
                              size_t max, size_t *count);
int  wlt_tor_circuit_lookup_nodes(wlt_tor_control_t *ctl,
                                  wlt_tor_circuit_t *circ);
const char *wlt_tor_status_to_str(wlt_tor_status_t s);
const char *wlt_tor_killswitch_to_str(wlt_tor_killswitch_t k);
const char *wlt_tor_node_flag_str(uint32_t flags, char *buf, size_t len);

/* ---- Identity (identity.c) --------------------------------------------- */
int  wlt_tor_new_identity(wlt_tor_control_t *ctl);
int  wlt_tor_clear_cookies(const char *cookie_jar_dir);

/* ---- Bridges (bridges.c) ----------------------------------------------- */
int  wlt_tor_bridges_add(wlt_tor_config_t *cfg, const wlt_tor_bridge_t *br);
int  wlt_tor_bridges_remove(wlt_tor_config_t *cfg, const char *addr);
int  wlt_tor_bridges_list(const wlt_tor_config_t *cfg, wlt_tor_bridge_t *out,
                          size_t max, size_t *count);
int  wlt_tor_bridges_apply(wlt_tor_control_t *ctl, const wlt_tor_config_t *cfg);
int  wlt_tor_bridge_parse(const char *line, wlt_tor_bridge_t *out);

/* ---- Kill switch (tor_mgr.c) ------------------------------------------- */
int                  wlt_tor_kill_switch_trigger(void);
int                  wlt_tor_kill_switch_release(void);
wlt_tor_killswitch_t wlt_tor_kill_switch_state(void);

/* ---- Main manager (tor_mgr.c) ------------------------------------------ */
typedef struct wlt_tor_mgr wlt_tor_mgr_t;

int  wlt_tor_mgr_init(wlt_tor_mgr_t **mgr, const wlt_tor_config_t *cfg);
void wlt_tor_mgr_shutdown(wlt_tor_mgr_t *mgr);
int  wlt_tor_mgr_run(wlt_tor_mgr_t *mgr);           /* blocking event loop  */
int  wlt_tor_mgr_stop(wlt_tor_mgr_t *mgr);
int  wlt_tor_mgr_get_status(wlt_tor_mgr_t *mgr,
                            wlt_tor_status_snapshot_t *out);
int  wlt_tor_mgr_get_circuits(wlt_tor_mgr_t *mgr, wlt_tor_circuit_t *out,
                              size_t max, size_t *count);
int  wlt_tor_mgr_new_identity(wlt_tor_mgr_t *mgr);
int  wlt_tor_mgr_set_exit(wlt_tor_mgr_t *mgr,
                          const char *country /* NULL => auto */);
int  wlt_tor_mgr_get_bandwidth(wlt_tor_mgr_t *mgr, wlt_tor_bandwidth_t *out);
const wlt_tor_config_t *wlt_tor_mgr_config(wlt_tor_mgr_t *mgr);

/* ---- CLI --------------------------------------------------------------- */
int  wlt_tor_cli_main(int argc, char **argv);

/* ---- Logging ----------------------------------------------------------- */
typedef enum {
    WLT_TOR_LOG_DEBUG = 0,
    WLT_TOR_LOG_INFO,
    WLT_TOR_LOG_WARN,
    WLT_TOR_LOG_ERROR
} wlt_tor_log_level_t;

void wlt_tor_log(wlt_tor_log_level_t lvl, const char *fmt, ...)
        __attribute__((format(printf, 2, 3)));
void wlt_tor_log_set_level(wlt_tor_log_level_t lvl);
void wlt_tor_log_set_quiet(bool quiet);

#define wlt_log_dbg(...) wlt_tor_log(WLT_TOR_LOG_DEBUG, __VA_ARGS__)
#define wlt_log_inf(...) wlt_tor_log(WLT_TOR_LOG_INFO,  __VA_ARGS__)
#define wlt_log_wrn(...) wlt_tor_log(WLT_TOR_LOG_WARN,  __VA_ARGS__)
#define wlt_log_err(...) wlt_tor_log(WLT_TOR_LOG_ERROR, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* WLT_TOR_MGR_H */
