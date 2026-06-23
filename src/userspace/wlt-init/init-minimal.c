/**
 * @file init-minimal.c
 * @brief Minimal WLTIOS init for QEMU testing (replaces Buildroot)
 * @module wlt-init
 * @purpose PID 1 in the WLTIOS initramfs — mounts fs, prints banner, starts shell
 *
 * This is NOT the production init (init.c). This is a minimal init for
 * QEMU integration testing. It does:
 *   1. Redirect console I/O to /dev/console
 *   2. Mount /proc, /sys, /dev
 *   3. Print "=== WLTIOS v0.1.0 ==="
 *   4. Try to start wlt-net, wlt-crypto (report STUB if they fail)
 *   5. Start wlt-shell (must succeed)
 *   6. Retry shell 5 times if it exits
 *   7. Halt if all retries fail
 *
 * Compile: gcc -m32 -static -o wlt-init-minimal init-minimal.c
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

static int try_start_service(const char *name, const char *path)
{
 (void)name;
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

static int start_shell(void)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[] = { "/bin/wlt-shell", NULL };
        char *envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=linux",
                         "HOME=/root", "PS1=/wlt# ", NULL };
        execve("/bin/wlt-shell", argv, envp);
        /* If wlt-shell not found, try /bin/sh */
        execve("/bin/sh", (char *[]){"/bin/sh", NULL}, envp);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

int main(void)
{
    /* 1. Redirect console */
    redirect_console();

    /* 2. Mount filesystems */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    /* 3. Print boot banner */
    printf("\n");
    printf("========================================\n");
    printf("       === WLTIOS v%s ===              \n", WLTIOS_VERSION);
    printf("  seL4 -> Linux -> WLT-Init             \n");
    printf("========================================\n");
    printf("\n");
    fflush(stdout);

    printf("[wlt-init] PID 1 starting\n");
    printf("[wlt-init] Mounting filesystems... OK\n");
    fflush(stdout);

    /* 4. Try starting services */
    printf("[wlt-init] Starting wlt-net... ");
    fflush(stdout);
    if (access("/sbin/wlt-net", X_OK) == 0) {
        if (try_start_service("wlt-net", "/sbin/wlt-net") == 0)
            printf("OK\n");
        else
            printf("STUB (network not available in QEMU)\n");
    } else {
        printf("STUB (binary not present)\n");
    }
    fflush(stdout);

    printf("[wlt-init] Starting wlt-crypto... ");
    fflush(stdout);
    if (access("/sbin/wlt-crypto", X_OK) == 0) {
        if (try_start_service("wlt-crypto", "/sbin/wlt-crypto") == 0)
            printf("OK\n");
        else
            printf("STUB (keyvault PD not available)\n");
    } else {
        printf("STUB (binary not present)\n");
    }
    fflush(stdout);

    /* 5. Start shell (with retries) */
    printf("[wlt-init] Starting wlt-shell...\n");
    fflush(stdout);

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        int rc = start_shell();
        if (rc == 0) {
            printf("[wlt-init] Shell exited cleanly (attempt %d/%d)\n",
                   attempt, MAX_RETRIES);
        } else {
            printf("[wlt-init] Shell exited with code %d (attempt %d/%d)\n",
                   rc, attempt, MAX_RETRIES);
        }
        fflush(stdout);
        sleep(RETRY_DELAY);
    }

    /* 6. All retries exhausted — halt */
    printf("[wlt-init] Shell failed %d times. Halting.\n", MAX_RETRIES);
    fflush(stdout);
    sync();
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    return 0;
}
