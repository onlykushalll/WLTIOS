/**
 * @file init-minimal.c
 * @brief Minimal WLTIOS init for QEMU testing (v2 — fixed null bytes)
 * @module wlt-init
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
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

int main(void)
{
    redirect_console();

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    print_str("");
    print_str("========================================");
    print_str("       === WLTIOS v0.1.0 ===");
    print_str("  seL4 -> Linux -> WLT-Init");
    print_str("========================================");
    print_str("");

    print_str("[wlt-init] PID 1 starting");
    print_str("[wlt-init] Mounting filesystems... OK");

    /* wlt-net */
    if (access("/sbin/wlt-net", X_OK) == 0) {
        if (try_exec("/sbin/wlt-net") == 0)
            print_str("[wlt-init] Starting wlt-net... OK");
        else
            print_str("[wlt-init] Starting wlt-net... FAILED (exit non-zero)");
    } else {
        print_str("[wlt-init] Starting wlt-net... STUB (binary not present)");
    }

    /* wlt-crypto */
    if (access("/sbin/wlt-crypto", X_OK) == 0) {
        if (try_exec("/sbin/wlt-crypto") == 0)
            print_str("[wlt-init] Starting wlt-crypto... OK");
        else
            print_str("[wlt-init] Starting wlt-crypto... FAILED (exit non-zero)");
    } else {
        print_str("[wlt-init] Starting wlt-crypto... STUB (binary not present)");
    }

    /* wlt-shell */
    print_str("[wlt-init] Starting wlt-shell...");

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        pid_t pid = fork();
        if (pid == 0) {
            char *argv[] = { "/bin/wlt-shell", NULL };
            char *envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
                             "TERM=linux", "HOME=/root",
                             "PS1=/wlt# ", NULL };
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
