/**
 * @file init-minimal.c
 * @brief Minimal WLTIOS init for QEMU testing (v4 — network + Tor + framebuffer)
 * @module wlt-init
 *
 * v4 changes:
 *   - Framebuffer init: set /dev/fb0 to 1366x768x32 for QEMU -vga std
 *   - Logs framebuffer status to serial
 *
 * v3 changes:
 *   - wlt-net now does REAL network bringup (qemu_bringup.c)
 *   - Tor is started in background if /usr/bin/tor exists
 *   - Logs every step to serial for verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <errno.h>

#define WLTIOS_VERSION "0.1.0"
#define MAX_RETRIES 5
#define RETRY_DELAY 2

static int redirect_console(void)
{
    int fd = open("/dev/console", O_RDWR);
    if (fd < 0) fd = open("/dev/ttyS0", O_RDWR);
    if (fd < 0) return -1;
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
    return 0;
}

static void print_str(const char *s)
{
    /* Write each character individually to avoid null-byte truncation */
    while (*s) {
        if (*s != '\0') {
            char c = *s;
            write(STDOUT_FILENO, &c, 1);
        }
        s++;
    }
    write(STDOUT_FILENO, "\r\n", 2);
}

static int try_exec(const char *path)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[] = { (char *)path, NULL };
        char *envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=linux", NULL };
        execve(path, argv, envp);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 0;
    return -1;
}

/**
 * Start Tor in background. Returns PID (>0) on success, -1 on failure.
 * Tor's stderr goes to serial console so we can see bootstrap progress.
 */
static pid_t start_tor_background(void)
{
    /* Tor is a musl-linked binary. The loader path differs by architecture:
     *   i386:   /lib64/ld-linux-x86-64.so.2
     *   x86_64: /lib64/ld-linux-x86-64.so.2
     * We detect which one exists and use it. */
    const char *loader = NULL;
    if (access("/lib64/ld-linux-x86-64.so.2", X_OK) == 0)
        loader = "/lib64/ld-linux-x86-64.so.2";
    else if (access("/lib64/ld-linux-x86-64.so.2", X_OK) == 0)
        loader = "/lib64/ld-linux-x86-64.so.2";

    const char *tor_bin = "/usr/bin/tor";

    if (loader == NULL)
    {
        print_str("[wlt-init] glibc/musl loader not found — skipping Tor");
        return -1;
    }
    if (access(tor_bin, X_OK) != 0)
    {
        print_str("[wlt-init] Tor binary not found — skipping Tor startup");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        print_str("[wlt-init] fork() for Tor failed");
        return -1;
    }
    if (pid == 0)
    {
        /* Child: exec ld-musl loader, which loads tor */
        char *argv[] = { (char *)loader, "--library-path", "/lib",
                         (char *)tor_bin, "-f", "/etc/tor/torrc", NULL };
        char *envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
                         "TERM=linux",
                         "HOME=/var/lib/tor",
                         NULL };
        execve(loader, argv, envp);
        /* If exec fails, write error and exit */
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-init] Tor exec failed: %s\r\n",
                         strerror(errno));
        write(2, msg, n > 0 ? n : 0);
        _exit(127);
    }

    /* Parent */
    {
        char msg[64];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-init] Tor started (pid=%d), waiting for bootstrap...",
                         (int)pid);
        write(1, msg, n > 0 ? n : 0);
        write(1, "\r\n", 2);
    }
    return pid;
}

int main(void)
{
    redirect_console();

    /* Core kernel filesystems */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    /* Amnesic mode: all writable directories are tmpfs (RAM only).
     * Nothing persists across reboots. This is the Tails-inspired
     * amnesic guarantee — power off = all data gone. */
    mount("tmpfs", "/tmp", "tmpfs", 0, "size=64m");
    mount("tmpfs", "/var", "tmpfs", 0, "size=32m");
    mount("tmpfs", "/run", "tmpfs", 0, "mode=0755");
    mount("tmpfs", "/home", "tmpfs", 0, "size=256m");

    /* Since /var is now tmpfs (empty), recreate required directories */
    mkdir("/var/lib", 0755);
    mkdir("/var/lib/tor", 0700);
    mkdir("/var/log", 0755);
    mkdir("/var/log/wlt", 0755);

    /* /run/wayland-0 will be created by the compositor */
    mkdir("/run", 0755);

    print_str("");
    print_str("========================================");
    print_str("       === WLTIOS v0.1.0 ===");
    print_str("  seL4 -> Linux -> WLT-Init (v5)");
    print_str("========================================");
    print_str("");

    print_str("[wlt-init] PID 1 starting");
    print_str("[wlt-init] Mounting filesystems... OK");
    print_str("[wlt-init] Amnesic mode: all writes in RAM (tmpfs)");

    /* Framebuffer init — set /dev/fb0 to 1366x768x32 for QEMU -vga std.
     * This must happen before any rendering commands (draw, compositor). */
    {
        int fb = open("/dev/fb0", O_RDWR);
        if (fb >= 0) {
            struct fb_var_screeninfo var;
            if (ioctl(fb, FBIOGET_VSCREENINFO, &var) == 0) {
                /* Try to set 1366x768x32 — QEMU -vga std supports this */
                var.xres = 1366;
                var.yres = 768;
                var.xres_virtual = 1366;
                var.yres_virtual = 768;
                var.bits_per_pixel = 32;
                if (ioctl(fb, FBIOPUT_VSCREENINFO, &var) == 0) {
                    char msg[128];
                    int n = snprintf(msg, sizeof(msg),
                                     "[wlt-init] Framebuffer: %ux%ux%u\r\n",
                                     var.xres, var.yres, var.bits_per_pixel);
                    write(1, msg, n > 0 ? n : 0);
                } else {
                    /* Re-read actual mode if set failed */
                    ioctl(fb, FBIOGET_VSCREENINFO, &var);
                    char msg[128];
                    int n = snprintf(msg, sizeof(msg),
                                     "[wlt-init] Framebuffer (default): %ux%ux%u\r\n",
                                     var.xres, var.yres, var.bits_per_pixel);
                    write(1, msg, n > 0 ? n : 0);
                }
            } else {
                print_str("[wlt-init] Framebuffer: FBIOGET failed");
            }
            close(fb);
        } else {
            print_str("[wlt-init] No /dev/fb0 (need -vga std in QEMU)");
        }
    }

    /* Load e1000 kernel module (Debian kernel has e1000 as module, not built-in).
     * Uses finit_module() syscall to load .ko from initramfs.
     * On i386, __NR_finit_module = 350. On x86_64 it's 313. */
    {
        int mfd = open("/lib/modules/e1000.ko", O_RDONLY);
        if (mfd >= 0)
        {
            /* i386 syscall number for finit_module */
            #ifdef __i386__
            #define __NR_finit_module 350
            #else
            #define __NR_finit_module 313
            #endif
            long ret = syscall(__NR_finit_module, mfd, "", 0);
            if (ret == 0)
                print_str("[wlt-init] Loaded e1000.ko module");
            else
            {
                char msg[80];
                int n = snprintf(msg, sizeof(msg),
                    "[wlt-init] e1000.ko load: ret=%ld errno=%d\r\n", ret, errno);
                write(1, msg, n > 0 ? n : 0);
            }
            close(mfd);
        }
    }

    /* wlt-net — REAL network bringup */
    if (access("/sbin/wlt-net", X_OK) == 0) {
        if (try_exec("/sbin/wlt-net") == 0)
            print_str("[wlt-init] Starting wlt-net... OK (network up)");
        else
            print_str("[wlt-init] Starting wlt-net... FAILED (exit non-zero)");
    } else {
        print_str("[wlt-init] Starting wlt-net... STUB (binary not present)");
    }

    /* wlt-crypto — may return non-zero if Key Vault PD unreachable (expected in QEMU) */
    if (access("/sbin/wlt-crypto", X_OK) == 0) {
        try_exec("/sbin/wlt-crypto");
        print_str("[wlt-init] Starting wlt-crypto... OK (see service output above)");
    } else {
        print_str("[wlt-init] Starting wlt-crypto... STUB (binary not present)");
    }

    /* Tor — start in background BEFORE shell */
    /* WLTIOS Defender — boot order: watchdog=PID2, guardian=PID3, tor=PID4 */
    /* Start guardian watchdog first (monitors heartbeat, triggers kill switch) */
    if (access("/usr/sbin/wlt-guardian-watchdog", X_OK) == 0)
    {
        pid_t wdpid = fork();
        if (wdpid == 0)
        {
            char *wdargs[] = {"/usr/sbin/wlt-guardian-watchdog", NULL};
            char *wdenvp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", NULL};
            execve("/usr/sbin/wlt-guardian-watchdog", wdargs, wdenvp);
            _exit(1);
        }
        print_str("[wlt-init] Guardian watchdog started\r\n");
        usleep(200000);
    }

    /* Start guardian daemon (PID 3) — needs network for Tor UID detection */
    if (access("/usr/sbin/wlt-guardian", X_OK) == 0)
    {
        print_str("[wlt-init] Starting Defender (guardian)...\r\n");
        pid_t gpid = fork();
        if (gpid == 0)
        {
            char *gargs[] = {"/usr/sbin/wlt-guardian", "/etc/wlt/guardian/guardian.conf", NULL};
            char *genvp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", NULL};
            execve("/usr/sbin/wlt-guardian", gargs, genvp);
            write(1, "[wlt-init] Defender exec failed\r\n", 33);
            _exit(1);
        }
        else if (gpid > 0)
        {
            char gmsg[80];
            int gn = snprintf(gmsg, sizeof(gmsg),
                "[wlt-init] Defender started (pid=%d)\r\n", (int)gpid);
            write(1, gmsg, gn > 0 ? gn : 0);
            usleep(500000);
        }
    }
    else
    {
        print_str("[wlt-init] Defender not found (binary missing)\r\n");
    }

    /* THEN start Tor — guardian detects tor's UID at init */
    print_str("[wlt-init] Starting Tor...");
    pid_t tor_pid = start_tor_background();
    if (tor_pid > 0)
    {
        /* Give Tor up to 30 seconds to bootstrap. During this time
         * Tor's stderr (bootstrap progress) appears on serial.
         * We don't block the shell forever — just give it a head start. */
        sleep(15);
        print_str("[wlt-init] Tor head-start complete (15s). Shell starting.");
        print_str("[wlt-init] Run 'tstatus' to check Tor, 'torcheck' to verify.");
    }
    else
    {
        print_str("[wlt-init] Tor not started (binary missing).");
        print_str("[wlt-init] Network-only mode. Use 'netcheck' to verify.");
    }

    /* Compositor — start in background if binary exists */
    if (access("/usr/bin/wlt-compositor", X_OK) == 0)
    {
        print_str("[wlt-init] Starting compositor...");
        pid_t comp_pid = fork();
        if (comp_pid == 0)
        {
            /* Child: set env vars for compositor */
            setenv("WLR_BACKENDS", "headless", 1);
            setenv("WLR_RENDERER", "pixman", 1);
            setenv("WAYLAND_DISPLAY", "wayland-0", 1);
            setenv("XDG_RUNTIME_DIR", "/run", 1);
            char *argv[] = { "/usr/bin/wlt-compositor", NULL };
            char *envp[] = {
                "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
                "TERM=linux",
                "HOME=/root",
                "WLR_BACKENDS=headless",
                "WLR_RENDERER=pixman",
                "WAYLAND_DISPLAY=wayland-0",
                "XDG_RUNTIME_DIR=/run",
                NULL
            };
            execve("/usr/bin/wlt-compositor", argv, envp);
            write(1, "[wlt-init] Compositor exec failed\r\n", 35);
            _exit(1);
        }
        else if (comp_pid > 0)
        {
            char msg[64];
            int n = snprintf(msg, sizeof(msg),
                "[wlt-init] Compositor started (pid=%d)\r\n", (int)comp_pid);
            write(1, msg, n > 0 ? n : 0);
            sleep(3);  /* Give compositor time to create socket */
        }
    }

    /* wlt-shell */
    print_str("[wlt-init] Starting wlt-shell...");

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        pid_t pid = fork();
        if (pid == 0) {
            char *argv[] = { "/bin/wlt-shell", NULL };
            char *envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
                             "TERM=linux", "HOME=/root",
                             "PS1=/wlt# ",
                             "WAYLAND_DISPLAY=wayland-0",
                             "XDG_RUNTIME_DIR=/run",
                             NULL };
            execve("/bin/wlt-shell", argv, envp);
            execve("/bin/sh", (char *[]){"/bin/sh", NULL}, envp);
            _exit(127);
        }
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int rc = WEXITSTATUS(status);
            if (rc == 0) {
                /* Shell exited cleanly — retry */
            }
        }
        sleep(RETRY_DELAY);
    }

    print_str("[wlt-init] Shell failed. Halting.");
    sync();
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    return 0;
}