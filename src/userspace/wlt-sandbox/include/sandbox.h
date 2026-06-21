/**
 * @file sandbox.h
 * @brief Public API for WLT-Sandbox (app isolation manager)
 * @module wlt-sandbox
 * @purpose Isolates apps via cgroups v2 + namespaces + seccomp
 * @dependencies: manifest.h
 * @tested_by: tests/userspace/sandbox_test.c
 * @spec: docs/specs/wlt-sandbox.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_SANDBOX_H
#define WLT_SANDBOX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include "manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_SANDBOX_OK = 0,
    WLT_SANDBOX_ERR_INVALID_ARG     = -1,
    WLT_SANDBOX_ERR_NOT_FOUND       = -2,
    WLT_SANDBOX_ERR_IO              = -3,
    WLT_SANDBOX_ERR_NOMEM           = -4,
    WLT_SANDBOX_ERR_PERMISSION      = -5,
    WLT_SANDBOX_ERR_PARSE           = -6,
    WLT_SANDBOX_ERR_CGROUP          = -7,  /* cgroup write/creation failed  */
    WLT_SANDBOX_ERR_NAMESPACE       = -8,  /* unshare() or uid map failed   */
    WLT_SANDBOX_ERR_SECCOMP         = -9,  /* BPF install failed            */
    WLT_SANDBOX_ERR_FORK            = -10, /* fork()/clone() failed         */
    WLT_SANDBOX_ERR_TIMEOUT         = -11,
    WLT_SANDBOX_ERR_ALREADY_RUNNING = -12,
    WLT_SANDBOX_ERR_MANIFEST        = -13,
} wlt_sandbox_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_SANDBOX_VERSION             "1.0.0"
#define WLT_SANDBOX_MAX_APPS            64
#define WLT_SANDBOX_MAX_NAME            64
#define WLT_SANDBOX_MAX_PATH            256
#define WLT_SANDBOX_MAX_ARGS            512
#define WLT_SANDBOX_APP_DIR             "/etc/wlt/apps"
#define WLT_SANDBOX_CGROUP_ROOT         "/sys/fs/cgroup/wlt"
#define WLT_SANDBOX_LOG_DIR             "/var/log/wlt/sandbox"
#define WLT_SANDBOX_AUDIT_LOG           WLT_SANDBOX_LOG_DIR "/audit.log"
#define WLT_SANDBOX_SETUP_BUDGET_MS     50
#define WLT_SANDBOX_MAX_SECCOMP_RULES   256
#define WLT_SANDBOX_SHUTDOWN_GRACE_MS   5000
#define WLT_SANDBOX_OOM_POLL_MS         200

/* ─── Types ──────────────────────────────────────────── */

typedef enum {
    WLT_SANDBOX_STOPPED = 0,
    WLT_SANDBOX_STARTING,
    WLT_SANDBOX_RUNNING,
    WLT_SANDBOX_STOPPING,
    WLT_SANDBOX_FAILED,
    WLT_SANDBOX_OOM_KILLED,      /* cgroup OOM killer reaped the app */
    WLT_SANDBOX_SECCOMP_KILLED,  /* kernel SIGSYS on disallowed syscall */
} wlt_sandbox_state_t;

/* Per-app runtime record. Indexed by manifest name. The launcher
 * fills the manifest fields; the monitor updates the runtime fields
 * as the app lives and dies. */
typedef struct {
    char id[WLT_SANDBOX_MAX_NAME];            /* Manifest key ("firefox") */
    wlt_SandboxManifest manifest;
    pid_t pid;                                /* Sandboxed PID (host view) */
    pid_t init_pid;                           /* PID 1 inside the sandbox ns */
    wlt_sandbox_state_t state;
    uint64_t started_at;                      /* Unix epoch seconds */
    uint64_t exited_at;
    int exit_code;
    int signal;                               /* 0 if exited normally */
    char cgroup_path[WLT_SANDBOX_MAX_PATH];   /* /sys/fs/cgroup/wlt/<id> */
    uint32_t oom_events;                      /* Cumulative OOM events */
    uint32_t seccomp_violations;
} wlt_SandboxApp;

typedef struct {
    wlt_SandboxApp apps[WLT_SANDBOX_MAX_APPS];
    size_t app_count;
    char cgroup_root[WLT_SANDBOX_MAX_PATH];
    bool initialised;
} wlt_SandboxManager;

/* ─── Lifecycle ──────────────────────────────────────── */

/**
 * Initialise the sandbox manager. Creates the cgroup root if missing.
 */
wlt_sandbox_error_t wlt_sandbox_init(wlt_SandboxManager *m,
                                     const char *cgroup_root);

/**
 * Tear down all running sandboxes and free resources.
 * Sends SIGTERM then SIGKILL to any remaining apps.
 */
void wlt_sandbox_shutdown(wlt_SandboxManager *m);

/* ─── Per-App Setup (invoked by launcher in the child) ── */

/**
 * Create the cgroup directory for the app and apply resource limits
 * from the manifest. The caller's PID is NOT yet moved into the
 * cgroup — that happens in wlt_sandbox_cgroup_attach().
 */
wlt_sandbox_error_t wlt_sandbox_setup_cgroup(wlt_SandboxApp *app);

/**
 * Move the calling process into the app's cgroup. Must be called
 * after fork() in the child before exec().
 */
wlt_sandbox_error_t wlt_sandbox_cgroup_attach(const wlt_SandboxApp *app);

/**
 * Unshare all namespaces required by the manifest. Must be called
 * in the child after fork(). Network namespace is only created when
 * the manifest requests WLT_NET_NONE or WLT_NET_TOR_ONLY.
 */
wlt_sandbox_error_t wlt_sandbox_setup_namespaces(const wlt_SandboxManifest *m);

/**
 * Install the seccomp-BPF filter derived from the manifest's
 * allowed syscall groups. Must be the last setup step before exec()
 * — once installed it cannot be removed.
 */
wlt_sandbox_error_t wlt_sandbox_setup_seccomp(const wlt_SandboxManifest *m);

/**
 * Pivot_root into a minimal filesystem with only the paths the
 * manifest grants. user_home is /users/<user>; mounted RW only when
 * the manifest requests WLT_FS_HOME_RW.
 */
wlt_sandbox_error_t wlt_sandbox_setup_filesystem(const wlt_SandboxManifest *m,
                                                 const char *user_home);

/**
 * Tear down a single app's cgroup and clear its runtime state.
 * Safe to call on a stopped app (no-op).
 */
wlt_sandbox_error_t wlt_sandbox_teardown(wlt_SandboxApp *app);

/* ─── Lookup & Utility ───────────────────────────────── */

wlt_SandboxApp *wlt_sandbox_find(wlt_SandboxManager *m, const char *id);
wlt_SandboxApp *wlt_sandbox_find_by_pid(wlt_SandboxManager *m, pid_t pid);
const char *wlt_sandbox_state_str(wlt_sandbox_state_t s);
const char *wlt_sandbox_error_str(wlt_sandbox_error_t err);

/* ─── Launch (launch.c) ────────────────────────────────
 * Load a manifest, set up the cgroup in the parent, fork, and in the
 * child: attach to cgroup → unshare namespaces → mount filesystem →
 * install seccomp → execv. The parent records the PID and returns. */

/**
 * Launch an app identified by its manifest path.
 * @param m              Sandbox manager (must be initialised)
 * @param manifest_path  Path to .manifest file
 * @param user_home      /users/<user> path; NULL = no home mount
 * @param out_app        Receives the registered app record (may be NULL)
 */
wlt_sandbox_error_t wlt_sandbox_launch(wlt_SandboxManager *m,
                                       const char *manifest_path,
                                       const char *user_home,
                                       wlt_SandboxApp **out_app);

/**
 * Send a signal to a sandboxed app. No-op if the app isn't running.
 */
wlt_sandbox_error_t wlt_sandbox_kill(wlt_SandboxApp *app, int signal);

/* ─── Monitor (monitor.c) ──────────────────────────────
 * The monitor reaps exited children, reads cgroup memory.events to
 * count OOM kills, and inspects /proc/<pid>/status for seccomp
 * violations. wlt_sandbox_monitor_loop() blocks forever (used by the
 * daemon's main loop); wlt_sandbox_monitor_step() does one pass. */

wlt_sandbox_error_t wlt_sandbox_monitor_loop(wlt_SandboxManager *m);
wlt_sandbox_error_t wlt_sandbox_monitor_step(wlt_SandboxManager *m,
                                             int timeout_ms);

/**
 * Read the cgroup's memory.events for the app and refresh oom_count.
 * Exposed for tests; the monitor calls it automatically.
 */
wlt_sandbox_error_t wlt_sandbox_monitor_refresh_oom(wlt_SandboxApp *app);

/* ─── Audit (audit.c) ──────────────────────────────────
 * Append-only security log at WLT_SANDBOX_AUDIT_LOG. Lines look like:
 *   2026-06-22T13:01:07Z INFO  app=firefox pid=1234 event=launch
 * The audit file is opened with O_APPEND so concurrent writers (e.g.
 * the launcher and monitor) don't clobber each other. */

typedef enum {
    WLT_AUDIT_INFO    = 0,  /* Normal lifecycle event           */
    WLT_AUDIT_WARN    = 1,  /* Suspicious but allowed           */
    WLT_AUDIT_DENIED  = 2,  /* Permission denied by sandbox     */
    WLT_AUDIT_KILLED  = 3,  /* App killed by OOM/seccomp        */
} wlt_audit_level_t;

wlt_sandbox_error_t wlt_audit_init(void);
void wlt_audit_close(void);
void wlt_audit_log(wlt_audit_level_t level, const char *app_id,
                   pid_t pid, const char *event, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* WLT_SANDBOX_H */
