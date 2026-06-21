/**
 * @file sandbox.c
 * @brief Sandbox setup: cgroups v2, namespaces, seccomp, filesystem
 * @module wlt-sandbox/core
 * @purpose Implements the kernel-level isolation primitives for sandboxed apps
 * @dependencies: sandbox.h, manifest.h, linux/cgroup v2, seccomp-BPF
 * @tested_by: tests/userspace/sandbox_test.c
 * @spec: docs/specs/wlt-sandbox.md
 * @adr: docs/adr/07-custom-init.md
 */

#include "sandbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <asm/unistd.h>

/* ─── Logging ────────────────────────────────────────── */
#define WLT_SB_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_SB_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox][WRN] " fmt "\n", ##__VA_ARGS__)
#define WLT_SB_LOG_INF(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox][INF] " fmt "\n", ##__VA_ARGS__)
#define WLT_SB_LOG_DBG(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox][DBG] " fmt "\n", ##__VA_ARGS__)

/* sizeof(arr)/sizeof(arr[0]) without risking double-evaluation. */
#define ARRAY_SIZE_(a) (sizeof(a) / sizeof((a)[0]))

/* Write a small string to a cgroup control file. All cgroup v2
 * configuration happens through such writes, so this helper is used
 * heavily below. */
static wlt_sandbox_error_t
write_file(const char *path, const char *content)
{
    int fd = open(path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd < 0)
    {
        WLT_SB_LOG_ERR("open(%s): %s", path, strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }
    ssize_t n = write(fd, content, strlen(content));
    int saved = errno;
    close(fd);
    if (n < 0)
    {
        WLT_SB_LOG_ERR("write(%s): %s", path, strerror(saved));
        return WLT_SANDBOX_ERR_IO;
    }
    return WLT_SANDBOX_OK;
}

static wlt_sandbox_error_t
mkdir_eexist_ok(const char *path, mode_t mode)
{
    if (mkdir(path, mode) == 0 || errno == EEXIST)
        return WLT_SANDBOX_OK;
    WLT_SB_LOG_ERR("mkdir(%s): %s", path, strerror(errno));
    return WLT_SANDBOX_ERR_CGROUP;
}

/* Apply manifest defaults so cgroup setup never writes a zero limit */
static void
resolve_resources(const wlt_SandboxManifest *m, wlt_SandboxResources *out)
{
    *out = m->resources;
    if (out->memory_mb == 0)   out->memory_mb = WLT_MANIFEST_DEFAULT_MEM_MB;
    if (out->cpu_percent == 0) out->cpu_percent = WLT_MANIFEST_DEFAULT_CPU_PCT;
    if (out->disk_mb == 0)     out->disk_mb = WLT_MANIFEST_DEFAULT_DISK_MB;
    if (out->pids == 0)        out->pids = WLT_MANIFEST_DEFAULT_PIDS;
}

/* ─── Cgroup v2 Setup ────────────────────────────────── */

wlt_sandbox_error_t
wlt_sandbox_setup_cgroup(wlt_SandboxApp *app)
{
    if (app == NULL || app->id[0] == '\0')
        return WLT_SANDBOX_ERR_INVALID_ARG;
    char path[WLT_SANDBOX_MAX_PATH];
    int n = snprintf(path, sizeof(path), "%s/%s",
                     WLT_SANDBOX_CGROUP_ROOT, app->id);
    if (n < 0 || (size_t)n >= sizeof(path))
        return WLT_SANDBOX_ERR_INVALID_ARG;
    snprintf(app->cgroup_path, sizeof(app->cgroup_path), "%s", path);
    wlt_sandbox_error_t e = mkdir_eexist_ok(path, 0755);
    if (e != WLT_SANDBOX_OK)
        return e;
    wlt_SandboxResources r;
    resolve_resources(&app->manifest, &r);

    char buf[128];
    /* +32 headroom for the longest suffix ("/memory.oom.group"). */
    char file[WLT_SANDBOX_MAX_PATH + 32];

    /* memory.max — bytes; cpu.max — "<quota> <period>" (period fixed
     * at 100000us); pids.max — process count cap. */
    snprintf(file, sizeof(file), "%s/memory.max", path);
    snprintf(buf, sizeof(buf), "%llu\n",
             (unsigned long long)r.memory_mb * 1024 * 1024);
    if ((e = write_file(file, buf)) != WLT_SANDBOX_OK) return e;

    snprintf(file, sizeof(file), "%s/cpu.max", path);
    snprintf(buf, sizeof(buf), "%llu 100000\n",
             (unsigned long long)r.cpu_percent * 1000);
    if ((e = write_file(file, buf)) != WLT_SANDBOX_OK) return e;

    snprintf(file, sizeof(file), "%s/pids.max", path);
    snprintf(buf, sizeof(buf), "%u\n", r.pids);
    if ((e = write_file(file, buf)) != WLT_SANDBOX_OK) return e;

    /* memory.oom.group — kill the whole cgroup on OOM, not just one
     * process. Prevents fork-bombs surviving by spreading memory. */
    snprintf(file, sizeof(file), "%s/memory.oom.group", path);
    if (write_file(file, "1\n") != WLT_SANDBOX_OK)
        WLT_SB_LOG_WRN("memory.oom.group not supported on this kernel");

    WLT_SB_LOG_INF("cgroup %s ready (mem=%uMB cpu=%u%% pids=%u)",
                   app->id, r.memory_mb, r.cpu_percent, r.pids);
    return WLT_SANDBOX_OK;
}

wlt_sandbox_error_t
wlt_sandbox_cgroup_attach(const wlt_SandboxApp *app)
{
    if (app == NULL || app->cgroup_path[0] == '\0')
        return WLT_SANDBOX_ERR_INVALID_ARG;
    char file[WLT_SANDBOX_MAX_PATH + 32];
    snprintf(file, sizeof(file), "%s/cgroup.procs", app->cgroup_path);
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", (int)getpid());
    return write_file(file, pid_str);
}

wlt_sandbox_error_t
wlt_sandbox_teardown(wlt_SandboxApp *app)
{
    if (app == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    if (app->cgroup_path[0] == '\0')
        return WLT_SANDBOX_OK;
    /* cgroup v2 refuses rmdir while it contains live processes —
     * the launcher must reap the app first. */
    if (rmdir(app->cgroup_path) != 0 && errno != ENOENT)
    {
        WLT_SB_LOG_WRN("rmdir(%s): %s", app->cgroup_path, strerror(errno));
        return WLT_SANDBOX_ERR_CGROUP;
    }
    app->cgroup_path[0] = '\0';
    app->state = WLT_SANDBOX_STOPPED;
    return WLT_SANDBOX_OK;
}

/* ─── Namespace Setup ────────────────────────────────── */

static wlt_sandbox_error_t
write_id_map(const char *which, pid_t pid, const char *map)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/%s", pid, which);
    return write_file(path, map);
}

wlt_sandbox_error_t
wlt_sandbox_setup_namespaces(const wlt_SandboxManifest *m)
{
    if (m == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    /* User namespace first — once we have it the others don't need
     * root. CLONE_NEWNS must come after CLONE_NEWUSER or the kernel
     * would propagate our root mounts to other tasks. */
    int flags = CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWPID |
                CLONE_NEWIPC | CLONE_NEWUTS;
    if (m->network != WLT_NET_DIRECT)
        flags |= CLONE_NEWNET;
    if (unshare(flags) != 0)
    {
        WLT_SB_LOG_ERR("unshare(0x%x): %s", flags, strerror(errno));
        return WLT_SANDBOX_ERR_NAMESPACE;
    }
    /* Map our UID/GID to root inside the user namespace. unshare(NEWPID)
     * only affects children, so we still see ourselves in /proc. */
    pid_t self = getpid();
    if (write_id_map("setgroups", self, "deny\n") != WLT_SANDBOX_OK ||
        write_id_map("uid_map", self, "0 1000 1\n") != WLT_SANDBOX_OK ||
        write_id_map("gid_map", self, "0 1000 1\n") != WLT_SANDBOX_OK)
        return WLT_SANDBOX_ERR_NAMESPACE;
    /* Recursively mark all mounts private so bind mounts don't leak
     * back to the host mount table. */
    if (mount("", "/", "", MS_REC | MS_PRIVATE, NULL) != 0)
    {
        WLT_SB_LOG_ERR("mount(--make-rprivate /): %s", strerror(errno));
        return WLT_SANDBOX_ERR_NAMESPACE;
    }
    WLT_SB_LOG_DBG("namespaces ready (net=%s)", wlt_manifest_net_str(m->network));
    return WLT_SANDBOX_OK;
}

/* ─── Seccomp ──────────────────────────────────────────
 * We build a BPF program at runtime from the manifest's allowed syscall
 * groups: arch-check → load nr → allow per syscall → default KILL. */

static const int syscalls_basic[] = {
    __NR_read, __NR_write, __NR_open, __NR_openat, __NR_close, __NR_stat,
    __NR_fstat, __NR_newfstatat, __NR_lseek, __NR_mmap, __NR_munmap,
    __NR_mprotect, __NR_mremap, __NR_brk, __NR_rt_sigprocmask,
    __NR_rt_sigaction, __NR_rt_sigreturn, __NR_exit, __NR_exit_group,
    __NR_getpid, __NR_gettid, __NR_prlimit64, __NR_getrandom,
    __NR_clock_gettime, __NR_fcntl,
};
static const int syscalls_network[] = {
    __NR_socket, __NR_connect, __NR_bind, __NR_listen, __NR_accept,
    __NR_accept4, __NR_sendto, __NR_recvfrom, __NR_sendmsg, __NR_recvmsg,
    __NR_shutdown, __NR_setsockopt, __NR_getsockopt, __NR_getsockname,
    __NR_getpeername,
};
static const int syscalls_filesystem[] = {
    __NR_statx, __NR_getdents64, __NR_readlink, __NR_readlinkat,
    __NR_access, __NR_faccessat, __NR_faccessat2, __NR_rename, __NR_renameat,
    __NR_renameat2, __NR_unlink, __NR_unlinkat, __NR_mkdir, __NR_mkdirat,
    __NR_rmdir, __NR_chmod, __NR_fchmod, __NR_fchmodat, __NR_truncate,
    __NR_ftruncate, __NR_fsync,
};
static const int syscalls_process[] = {
    __NR_fork, __NR_vfork, __NR_clone, __NR_wait4, __NR_getppid,
    __NR_getuid, __NR_geteuid, __NR_getgid, __NR_getegid,
    __NR_set_tid_address, __NR_set_robust_list, __NR_execve, __NR_arch_prctl,
};
static const int syscalls_signal[] = {
    __NR_rt_sigpending, __NR_rt_sigtimedwait, __NR_sigaltstack, __NR_kill,
    __NR_tgkill, __NR_tkill,
};
static const int syscalls_ipc[] = {
    __NR_pipe, __NR_pipe2, __NR_shmget, __NR_shmat, __NR_shmdt, __NR_shmctl,
    __NR_semget, __NR_semop, __NR_semctl, __NR_msgget, __NR_msgsnd,
    __NR_msgrcv, __NR_msgctl,
};

struct syscall_group { const int *syscalls; size_t count; };
static const struct syscall_group syscall_groups[] = {
    [WLT_SECCOMP_GRP_BASIC]      = { syscalls_basic,      ARRAY_SIZE_(syscalls_basic) },
    [WLT_SECCOMP_GRP_NETWORK]    = { syscalls_network,    ARRAY_SIZE_(syscalls_network) },
    [WLT_SECCOMP_GRP_FILESYSTEM] = { syscalls_filesystem, ARRAY_SIZE_(syscalls_filesystem) },
    [WLT_SECCOMP_GRP_PROCESS]    = { syscalls_process,    ARRAY_SIZE_(syscalls_process) },
    [WLT_SECCOMP_GRP_SIGNAL]     = { syscalls_signal,     ARRAY_SIZE_(syscalls_signal) },
    [WLT_SECCOMP_GRP_IPC]        = { syscalls_ipc,        ARRAY_SIZE_(syscalls_ipc) },
};

static wlt_sandbox_error_t
build_seccomp_filter(const wlt_SandboxManifest *m,
                     struct sock_filter *out, size_t out_max, size_t *out_len)
{
    size_t n = 0;
#define EMIT(stmt) do { \
    if (n >= out_max) return WLT_SANDBOX_ERR_SECCOMP; \
    struct sock_filter _tmp = stmt; \
    out[n++] = _tmp; \
} while (0)
    /* Arch check — kill if not x86_64. Cross-arch syscalls let an
     * attacker bypass x86_64 filters via int 0x80. */
    EMIT(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)));
    EMIT(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0));
    EMIT(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));
    EMIT(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)));
    /* One allow-rule per whitelisted syscall */
    for (int g = 0; g < WLT_SECCOMP_GRP_COUNT; g++)
    {
        if ((m->seccomp_groups & (1u << g)) == 0)
            continue;
        const struct syscall_group *grp = &syscall_groups[g];
        for (size_t i = 0; i < grp->count; i++)
        {
            EMIT(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                          (uint32_t)grp->syscalls[i], 0, 1));
            EMIT(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
        }
    }
    /* Default: KILL_PROCESS so a forked child can't inherit the
     * filter and probe syscalls. */
    EMIT(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));
#undef EMIT
    *out_len = n;
    return WLT_SANDBOX_OK;
}

wlt_sandbox_error_t
wlt_sandbox_setup_seccomp(const wlt_SandboxManifest *m)
{
    if (m == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    struct sock_filter filter[WLT_SANDBOX_MAX_SECCOMP_RULES];
    size_t flen = 0;
    wlt_sandbox_error_t e = build_seccomp_filter(m, filter,
                                                 WLT_SANDBOX_MAX_SECCOMP_RULES,
                                                 &flen);
    if (e != WLT_SANDBOX_OK)
        return e;
    struct sock_fprog prog = { .len = (unsigned short)flen, .filter = filter };
    /* PR_SET_NO_NEW_PRIVS is mandatory before seccomp for non-root. */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
    {
        WLT_SB_LOG_ERR("prctl(NO_NEW_PRIVS): %s", strerror(errno));
        return WLT_SANDBOX_ERR_SECCOMP;
    }
    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0)
    {
        WLT_SB_LOG_ERR("seccomp(): %s", strerror(errno));
        return WLT_SANDBOX_ERR_SECCOMP;
    }
    WLT_SB_LOG_DBG("seccomp filter installed (%zu rules)", flen);
    return WLT_SANDBOX_OK;
}

/* ─── Filesystem Setup ─────────────────────────────────
 * Build a minimal mount table on top of the host's /: /usr (ro),
 * /users/<user> (rw if granted), /tmp (private tmpfs), /dev (limited).
 * pivot_root is omitted — it needs a separate staging dir and adds
 * failure modes not justified until we ship a per-app rootfs. */
static wlt_sandbox_error_t
mount_bind(const char *src, const char *dst, unsigned long flags)
{
    if (mount(src, dst, NULL, MS_BIND | MS_REC, NULL) != 0)
    {
        WLT_SB_LOG_ERR("bind mount %s -> %s: %s", src, dst, strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }
    /* Remount to apply flags (MS_BIND ignores most flags on first mount) */
    if (flags != 0)
    {
        if (mount(NULL, dst, NULL, MS_REMOUNT | MS_BIND | flags, NULL) != 0)
        {
            WLT_SB_LOG_ERR("remount %s: %s", dst, strerror(errno));
            return WLT_SANDBOX_ERR_IO;
        }
    }
    return WLT_SANDBOX_OK;
}

static wlt_sandbox_error_t
ensure_dir(const char *path, mode_t mode)
{
    if (mkdir(path, mode) != 0 && errno != EEXIST)
    {
        WLT_SB_LOG_ERR("mkdir(%s): %s", path, strerror(errno));
        return WLT_SANDBOX_ERR_IO;
    }
    return WLT_SANDBOX_OK;
}

wlt_sandbox_error_t
wlt_sandbox_setup_filesystem(const wlt_SandboxManifest *m,
                             const char *user_home)
{
    if (m == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    /* Private /tmp; /usr read-only bind; /dev via devtmpfs with
     * tmpfs fallback. User home only mounted if manifest grants it. */
    if (mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "size=64m") != 0)
        WLT_SB_LOG_WRN("private /tmp mount: %s", strerror(errno));
    if (mount_bind("/usr", "/usr", MS_RDONLY | MS_NODEV | MS_NOSUID) != WLT_SANDBOX_OK)
        return WLT_SANDBOX_ERR_IO;
    if (m->filesystem == WLT_FS_HOME_RO || m->filesystem == WLT_FS_HOME_RW)
    {
        if (user_home == NULL || user_home[0] == '\0')
            WLT_SB_LOG_WRN("home requested but no user_home provided");
        else
        {
            unsigned long flags = (m->filesystem == WLT_FS_HOME_RO)
                                  ? MS_RDONLY | MS_NODEV : MS_NODEV;
            if (ensure_dir(user_home, 0700) != WLT_SANDBOX_OK ||
                mount_bind(user_home, user_home, flags) != WLT_SANDBOX_OK)
                return WLT_SANDBOX_ERR_IO;
        }
    }
    if (mount("devtmpfs", "/dev", "devtmpfs",
              MS_NOSUID | MS_NOEXEC, "mode=0755") != 0)
        mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_NOEXEC, "size=4m");
    WLT_SB_LOG_DBG("filesystem ready (fs=%s)", wlt_manifest_fs_str(m->filesystem));
    return WLT_SANDBOX_OK;
}

/* ─── Manager Lifecycle ──────────────────────────────── */

wlt_sandbox_error_t
wlt_sandbox_init(wlt_SandboxManager *m, const char *cgroup_root)
{
    if (m == NULL)
        return WLT_SANDBOX_ERR_INVALID_ARG;
    memset(m, 0, sizeof(*m));
    const char *root = (cgroup_root != NULL) ? cgroup_root
                                             : WLT_SANDBOX_CGROUP_ROOT;
    snprintf(m->cgroup_root, sizeof(m->cgroup_root), "%s", root);
    /* Per-app dirs request controllers implicitly by writing to
     * memory.max / cpu.max / pids.max, so no root-level enabling. */
    wlt_sandbox_error_t e = mkdir_eexist_ok(m->cgroup_root, 0755);
    if (e != WLT_SANDBOX_OK)
        return e;
    mkdir_eexist_ok(WLT_SANDBOX_LOG_DIR, 0755);
    m->initialised = true;
    WLT_SB_LOG_INF("sandbox manager ready (cgroup_root=%s)", m->cgroup_root);
    return WLT_SANDBOX_OK;
}

void
wlt_sandbox_shutdown(wlt_SandboxManager *m)
{
    if (m == NULL || !m->initialised)
        return;
    for (size_t i = 0; i < m->app_count; i++)
    {
        wlt_SandboxApp *app = &m->apps[i];
        if (app->state == WLT_SANDBOX_RUNNING && app->pid > 0)
        {
            WLT_SB_LOG_INF("shutdown: killing %s (pid %d)", app->id, app->pid);
            kill(app->pid, SIGTERM);
            /* Reap is left to the monitor's waitpid loop */
        }
        wlt_sandbox_teardown(app);
    }
    m->app_count = 0;
    m->initialised = false;
}

/* ─── Lookup & Utility ───────────────────────────────── */

wlt_SandboxApp *
wlt_sandbox_find(wlt_SandboxManager *m, const char *id)
{
    if (m == NULL || id == NULL)
        return NULL;
    for (size_t i = 0; i < m->app_count; i++)
        if (strcmp(m->apps[i].id, id) == 0)
            return &m->apps[i];
    return NULL;
}

wlt_SandboxApp *
wlt_sandbox_find_by_pid(wlt_SandboxManager *m, pid_t pid)
{
    if (m == NULL)
        return NULL;
    for (size_t i = 0; i < m->app_count; i++)
        if (m->apps[i].pid == pid)
            return &m->apps[i];
    return NULL;
}

const char *
wlt_sandbox_state_str(wlt_sandbox_state_t s)
{
    switch (s)
    {
        case WLT_SANDBOX_STOPPED:        return "stopped";
        case WLT_SANDBOX_STARTING:       return "starting";
        case WLT_SANDBOX_RUNNING:        return "running";
        case WLT_SANDBOX_STOPPING:       return "stopping";
        case WLT_SANDBOX_FAILED:         return "failed";
        case WLT_SANDBOX_OOM_KILLED:     return "oom-killed";
        case WLT_SANDBOX_SECCOMP_KILLED: return "seccomp-killed";
        default:                         return "unknown";
    }
}

const char *
wlt_sandbox_error_str(wlt_sandbox_error_t err)
{
    switch (err)
    {
        case WLT_SANDBOX_OK:                  return "ok";
        case WLT_SANDBOX_ERR_INVALID_ARG:     return "invalid argument";
        case WLT_SANDBOX_ERR_NOT_FOUND:       return "not found";
        case WLT_SANDBOX_ERR_IO:              return "io error";
        case WLT_SANDBOX_ERR_NOMEM:           return "out of memory";
        case WLT_SANDBOX_ERR_PERMISSION:      return "permission denied";
        case WLT_SANDBOX_ERR_PARSE:           return "parse error";
        case WLT_SANDBOX_ERR_CGROUP:          return "cgroup error";
        case WLT_SANDBOX_ERR_NAMESPACE:       return "namespace error";
        case WLT_SANDBOX_ERR_SECCOMP:         return "seccomp error";
        case WLT_SANDBOX_ERR_FORK:            return "fork failed";
        case WLT_SANDBOX_ERR_TIMEOUT:         return "timed out";
        case WLT_SANDBOX_ERR_ALREADY_RUNNING: return "already running";
        case WLT_SANDBOX_ERR_MANIFEST:        return "manifest error";
        default:                              return "unknown error";
    }
}
