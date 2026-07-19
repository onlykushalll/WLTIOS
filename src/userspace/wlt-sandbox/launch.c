#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/**
 * @file launch.c
 * @brief App launcher: fork, sandbox setup, exec
 * @module wlt-sandbox/launch
 * @purpose Starts an app inside an isolated cgroup/namespace/seccomp sandbox
 * @dependencies: sandbox.h, manifest.h, audit
 * @tested_by: tests/userspace/launch_test.c
 * @spec: docs/specs/wlt-sandbox.md
 */

#include "sandbox.h"
#include "permissions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

/* ─── Logging ────────────────────────────────────────── */
#define WLT_LAUNCH_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/launch][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_LAUNCH_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/launch][WRN] " fmt "\n", ##__VA_ARGS__)
#define WLT_LAUNCH_LOG_INF(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/launch][INF] " fmt "\n", ##__VA_ARGS__)
#define WLT_LAUNCH_LOG_DBG(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/launch][DBG] " fmt "\n", ##__VA_ARGS__)

/* Maximum argv entries passed to execv. Includes the program name
 * (argv[0]) and the terminating NULL, so 30 user args is the cap. */
#define WLT_LAUNCH_MAX_ARGV 32

/* ─── Helpers ────────────────────────────────────────── */

/* Bounded copy that always NUL-terminates. Avoids the strncpy
 * "no NUL if src longer than n" footgun. */
static void
copy_bounded(char *dst, size_t dst_max, const char *src)
{
    if (dst_max == 0)
        return;
    size_t n = strlen(src);
    if (n >= dst_max)
        n = dst_max - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Split the manifest's args string on whitespace into argv entries.
 * Whitespace inside quotes is preserved (apps pass URLs with spaces).
 * Returns the argc count, or -1 on overflow. */
static int
build_argv(const char *exec, const char *args, char *argv[],
           int argv_max, char *args_buf, size_t args_buf_max)
{
    int argc = 0;
    if (argc >= argv_max)
        return -1;
    argv[argc++] = (char *)exec;

    copy_bounded(args_buf, args_buf_max, args);
    char *p = args_buf;
    while (*p && argc < argv_max - 1)
    {
        /* Skip leading whitespace */
        while (*p && (*p == ' ' || *p == '\t'))
            p++;
        if (*p == '\0')
            break;

        char *token_start = p;
        if (*p == '"')
        {
            /* Quoted token — copy until matching quote */
            p++;
            token_start = p;
            while (*p && *p != '"')
                p++;
            if (*p == '"')
            {
                *p = '\0';
                p++;
            }
        }
        else
        {
            /* Unquoted — terminate at next whitespace */
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
            {
                *p = '\0';
                p++;
            }
        }
        argv[argc++] = token_start;
    }
    argv[argc] = NULL;
    return argc;
}

/* Resolve the user_home path. The spec uses /users/<user>; we accept
 * either an absolute path or a username (which we prefix with /users/). */
static void
resolve_user_home(const char *in, char *out, size_t out_max)
{
    if (in == NULL || *in == '\0')
    {
        out[0] = '\0';
        return;
    }
    if (in[0] == '/')
        copy_bounded(out, out_max, in);
    else
        snprintf(out, out_max, "/users/%s", in);
}

/* ─── Child Setup ──────────────────────────────────────
 * All functions in this section run in the forked child. Order matters:
 * cgroup attach → namespace unshare → filesystem → seccomp → exec.
 * Seccomp must be last because it locks us out of mount()/unshare(). */

static wlt_sandbox_error_t
child_setup(const wlt_SandboxApp *app, const char *user_home)
{
    wlt_sandbox_error_t e;

    /* Attach to the cgroup first — the parent already created it and
     * wrote the limits. Doing this before unshare() means the write
     * to cgroup.procs still works under the host's user namespace. */
    e = wlt_sandbox_cgroup_attach(app);
    if (e != WLT_SANDBOX_OK)
        return e;

    /* Create new namespaces. This must happen before filesystem
     * setup so the bind mounts land in the new mount namespace. */
    e = wlt_sandbox_setup_namespaces(&app->manifest);
    if (e != WLT_SANDBOX_OK)
        return e;

    e = wlt_sandbox_setup_filesystem(&app->manifest, user_home);
    if (e != WLT_SANDBOX_OK)
        return e;

    /* Seccomp last — once installed we cannot mount, unshare, or
     * open new cgroup files. The filter is inherited by exec'd code
     * and cannot be removed. */
    e = wlt_sandbox_setup_seccomp(&app->manifest);
    if (e != WLT_SANDBOX_OK)
        return e;

    return WLT_SANDBOX_OK;
}

/* Report a setup failure from the child to the parent via the pipe.
 * The parent reads 4 bytes (an int error code); a short read or zero
 * means "no error, exec proceeded". */
static void
report_child_error(int pipe_fd, wlt_sandbox_error_t err)
{
    if (pipe_fd < 0)
        return;
    int code = (int)err;
    ssize_t n = write(pipe_fd, &code, sizeof(code));
    (void)n;  /* Best effort — parent will see EOF regardless */
    close(pipe_fd);
}

/* ─── Child Path ───────────────────────────────────────
 * Runs in the forked child. Calls child_setup, builds argv, execv's.
 * Never returns — on any failure we report the error to the parent
 * via the status pipe and _exit(127). */
static void
run_child(const wlt_SandboxApp *app, const char *user_home,
          int read_fd, int write_fd)
{
    close(read_fd);  /* Child only writes to the status pipe */

    /* Drop the audit log fd — the child inherits the parent's open
     * files, but writing to the audit log from the child would race
     * with the parent. */
    wlt_audit_close();

    wlt_sandbox_error_t e = child_setup(app, user_home);
    if (e != WLT_SANDBOX_OK)
    {
        WLT_LAUNCH_LOG_ERR("child setup failed for %s: %s",
                           app->id, wlt_sandbox_error_str(e));
        report_child_error(write_fd, e);
        _exit(127);
    }

    /* Build argv and exec. After this point the only way to report
     * failure is the exit code — the parent waits for it. */
    char *argv[WLT_LAUNCH_MAX_ARGV];
    char args_buf[WLT_SANDBOX_MAX_ARGS];
    if (build_argv(app->manifest.exec, app->manifest.args,
                   argv, WLT_LAUNCH_MAX_ARGV,
                   args_buf, sizeof(args_buf)) < 0)
    {
        report_child_error(write_fd, WLT_SANDBOX_ERR_INVALID_ARG);
        _exit(127);
    }

    execv(app->manifest.exec, argv);
    /* execv only returns on failure */
    WLT_LAUNCH_LOG_ERR("execv(%s): %s", app->manifest.exec, strerror(errno));
    report_child_error(write_fd, WLT_SANDBOX_ERR_IO);
    _exit(127);
}

/* ─── Parent Path ──────────────────────────────────────
 * Read the status pipe: a 4-byte error code means the child failed
 * before exec (we reap and roll back); a short read or EOF means
 * exec succeeded and the child is now the app. */
static wlt_sandbox_error_t
reap_or_register(wlt_SandboxManager *m, wlt_SandboxApp *app, pid_t pid,
                 int status_pipe, wlt_SandboxApp **out_app)
{
    close(status_pipe);  /* Close write end — parent reads */
    int child_err = 0;
    ssize_t rn = read(status_pipe, &child_err, sizeof(child_err));
    close(status_pipe);

    if (rn == (ssize_t)sizeof(child_err))
    {
        /* Child failed before exec — reap it and roll back. */
        int status;
        waitpid(pid, &status, 0);
        WLT_LAUNCH_LOG_ERR("child setup failed for %s: %s", app->id,
                           wlt_sandbox_error_str((wlt_sandbox_error_t)child_err));
        wlt_sandbox_teardown(app);
        return (wlt_sandbox_error_t)child_err;
    }

    /* Success — register the app. */
    app->pid = pid;
    app->state = WLT_SANDBOX_RUNNING;
    app->started_at = (uint64_t)time(NULL);
    m->app_count++;

    wlt_audit_log(WLT_AUDIT_INFO, app->id, pid, "launch",
                  wlt_manifest_net_str(app->manifest.network));

    if (out_app != NULL)
        *out_app = app;

    WLT_LAUNCH_LOG_INF("launched %s (pid %d, cgroup %s)",
                       app->id, pid, app->cgroup_path);
    return WLT_SANDBOX_OK;
}

/* ─── Public API ─────────────────────────────────────── */

/* Reserve a slot in the manager, load the manifest, and prepare the
 * app record. Returns the prepared app (with cgroup_path set) on
 * success, or NULL with an error code in *err. */
static wlt_SandboxApp *
prepare_app_slot(wlt_SandboxManager *m, const char *manifest_path,
                 wlt_sandbox_error_t *err)
{
    if (m->app_count >= WLT_SANDBOX_MAX_APPS)
    {
        *err = WLT_SANDBOX_ERR_NOMEM;
        return NULL;
    }
    wlt_SandboxApp *app = &m->apps[m->app_count];
    memset(app, 0, sizeof(*app));

    wlt_manifest_error_t me = wlt_manifest_load(manifest_path, &app->manifest);
    if (me != WLT_MANIFEST_OK)
    {
        WLT_LAUNCH_LOG_ERR("manifest load failed: %s", manifest_path);
        *err = WLT_SANDBOX_ERR_MANIFEST;
        return NULL;
    }
    copy_bounded(app->id, sizeof(app->id), app->manifest.name);

    if (wlt_sandbox_find(m, app->id) != NULL)
    {
        WLT_LAUNCH_LOG_ERR("app %s already running", app->id);
        *err = WLT_SANDBOX_ERR_ALREADY_RUNNING;
        return NULL;
    }

    app->state = WLT_SANDBOX_STARTING;
    *err = wlt_sandbox_setup_cgroup(app);
    if (*err != WLT_SANDBOX_OK)
        return NULL;
    return app;
}

wlt_sandbox_error_t
wlt_sandbox_launch(wlt_SandboxManager *m, const char *manifest_path,
                   const char *user_home_in, wlt_SandboxApp **out_app)
{
    if (m == NULL || manifest_path == NULL || !m->initialised)
        return WLT_SANDBOX_ERR_INVALID_ARG;

    wlt_sandbox_error_t e;

    /* --- Permission check (wlt-permissions, C.2) ---
     * Check BEFORE cgroup setup so the denial fires even without root.
     * Loads the manifest inline to get app_id + network mode, then
     * checks the grant file. If DENY + manifest requests network,
     * block immediately. Unregistered apps pass through with audit log. */
    {
        wlt_SandboxManifest mfst;
        wlt_manifest_error_t me = wlt_manifest_load(manifest_path, &mfst);
        if (me != WLT_MANIFEST_OK) {
            WLT_LAUNCH_LOG_ERR("manifest load failed: %s", manifest_path);
            return WLT_SANDBOX_ERR_MANIFEST;
        }

        static wlt_PermsManager perms;
        static int perms_inited = 0;
        if (!perms_inited) {
            const char *grants_dir = getenv("WLT_PERMS_GRANTS_DIR");
            if (!grants_dir) grants_dir = WLT_PERMS_GRANTS_DIR;
            if (wlt_perms_init(&perms, grants_dir) == WLT_PERMS_OK) {
                perms_inited = 1;
            } else {
                WLT_LAUNCH_LOG_WRN("permission manager init failed; "
                                   "skipping network check");
            }
        }
        if (perms_inited) {
            /* Register the app first (loads manifest into manager),
             * then load its grants from disk. wlt_perms_register is
             * idempotent if already registered. */
            wlt_PermsManifest pmfst;
            wlt_perms_manifest_default(&pmfst);
            snprintf(pmfst.name, sizeof(pmfst.name), "%s", mfst.name);
            pmfst.values[WLT_PERM_NETWORK] = WLT_PERMS_NET_TOR_ONLY;
            pmfst.declared_mask |= (1u << WLT_PERM_NETWORK);
            wlt_perms_register(&perms, &pmfst);
            wlt_perms_load(&perms, mfst.name);
            wlt_PermsApp *papp = wlt_perms_find(&perms, mfst.name);
            if (papp != NULL) {
                wlt_perms_decision_t d =
                    wlt_perms_check(&perms, mfst.name, WLT_PERM_NETWORK);
                if (d == WLT_PERMS_DECISION_DENY &&
                    mfst.network != WLT_NET_NONE) {
                    WLT_LAUNCH_LOG_ERR("PERMISSION DENIED: %s requests network "
                                       "(%s) but grant is DENY",
                                       mfst.name,
                                       wlt_manifest_net_str(mfst.network));
                    wlt_perms_audit_log(WLT_PERMS_AUDIT_DENIED, mfst.name,
                                        WLT_PERM_NETWORK, "launch", "system",
                                        "network denied");
                    return WLT_SANDBOX_ERR_PERMISSION;
                }
                wlt_perms_audit_log(WLT_PERMS_AUDIT_GRANTED, mfst.name,
                                    WLT_PERM_NETWORK, "launch", "system",
                                    wlt_perms_decision_str(d));
            } else {
                wlt_perms_audit_log(WLT_PERMS_AUDIT_INFO, mfst.name,
                                    WLT_PERM_NETWORK, "launch", "system",
                                    "unregistered app");
            }
        }
    }

    wlt_SandboxApp *app = prepare_app_slot(m, manifest_path, &e);
    if (app == NULL)
        return e;

    char user_home[WLT_SANDBOX_MAX_PATH];
    resolve_user_home(user_home_in, user_home, sizeof(user_home));

    /* Status pipe: CLOEXEC so a successful execv() closes the write
     * end and the parent sees a clean EOF (zero-byte read). */
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) != 0)
    {
        WLT_LAUNCH_LOG_ERR("pipe2: %s", strerror(errno));
        wlt_sandbox_teardown(app);
        return WLT_SANDBOX_ERR_IO;
    }

    WLT_LAUNCH_LOG_INF("launching %s: %s %s",
                       app->id, app->manifest.exec, app->manifest.args);

    pid_t pid = fork();
    if (pid < 0)
    {
        WLT_LAUNCH_LOG_ERR("fork: %s", strerror(errno));
        close(pipefd[0]); close(pipefd[1]);
        wlt_sandbox_teardown(app);
        return WLT_SANDBOX_ERR_FORK;
    }
    if (pid == 0)
        run_child(app, user_home, pipefd[0], pipefd[1]);  /* never returns */

    return reap_or_register(m, app, pid, pipefd[0], out_app);
}

wlt_sandbox_error_t
wlt_sandbox_kill(wlt_SandboxApp *app, int signal)
{
    if (app == NULL || app->pid <= 0)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    if (kill(app->pid, signal) != 0)
    {
        WLT_LAUNCH_LOG_WRN("kill(%d, %d): %s",
                           app->pid, signal, strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }
    wlt_audit_log(WLT_AUDIT_WARN, app->id, app->pid, "signal", "sent");
    return WLT_SANDBOX_OK;
}
