/**
 * @file wlt-init-stub.c
 * @brief Minimal init wrapper for seL4 Linux guest — redirects console, retries
 * @module wlt-init
 * @purpose Replace Buildroot's init with a stub that connects serial console
 *
 * BUILD (static, in WSL2):
 *   gcc -static -o wlt-init-stub wlt-init-stub.c -Wall -Wextra -Werror
 *
 * USAGE:
 *   Inject as /sbin/init in the Buildroot cpio rootfs.
 *   Boot seL4 + Linux in QEMU with -serial stdio -append "console=ttyS0"
 *   The stub redirects stdin/stdout/stderr to /dev/console before exec-ing
 *   /bin/wlt-shell. If the shell exits, the stub retries (up to 5 times).
 *
 * ARCHITECTURE:
 *   This is NOT PID 1 in the seL4 sense. This runs inside the Linux guest VM
 *   as PID 1 (init). It:
 *     1. Opens /dev/console for stdin/stdout/stderr
 *     2. Prints "=== WLTIOS Boot ===" so we can see it in QEMU serial output
 *     3. Execs /bin/wlt-shell (our custom shell)
 *     4. If the shell exits (e.g., empty stdin), retries with a delay
 *     5. After 5 failures, drops to /bin/sh as fallback
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <errno.h>

#define WLT_SHELL_PATH  "/bin/wlt-shell"
#define FALLBACK_SHELL  "/bin/sh"
#define CONSOLE_DEV     "/dev/console"
#define MAX_RETRIES     5
#define RETRY_DELAY_SEC 2

static int redirect_console(void)
{
    int fd = open(CONSOLE_DEV, O_RDWR);
    if (fd < 0) {
        /* If /dev/console doesn't exist, try /dev/ttyS0 */
        fd = open("/dev/ttyS0", O_RDWR);
    }
    if (fd < 0) {
        return -1;
    }

    /* Redirect stdin, stdout, stderr to the console */
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) {
        close(fd);
    }
    return 0;
}

static int exec_shell(const char *path)
{
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        /* Child: exec the shell */
        char *argv[] = { (char *)path, NULL };
        char *envp[] = { "PATH=/bin:/usr/bin:/sbin:/usr/sbin", "TERM=linux", NULL };
        execve(path, argv, envp);
        /* If exec fails, exit with error */
        perror("execve failed");
        _exit(127);
    }
    /* Parent: wait for child */
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

int main(void)
{
    /* Step 1: Redirect console I/O */
    if (redirect_console() < 0) {
        /* Can't print anywhere yet — just try to continue */
    }

    /* Step 2: Print boot banner */
    printf("\n");
    printf("========================================\n");
    printf("       === WLTIOS Boot ===              \n");
    printf("  seL4 -> Linux -> WLT-Init-Stub        \n");
    printf("========================================\n");
    printf("\n");
    fflush(stdout);

    /* Step 3: Mount essential filesystems if not already mounted */
    /* These are safe to attempt even if already mounted (remount) */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    /* Step 4: Try to exec wlt-shell with retries */
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        printf("[wlt-init] Starting WLT-Shell (attempt %d/%d)...\n", attempt, MAX_RETRIES);
        fflush(stdout);

        int rc = exec_shell(WLT_SHELL_PATH);
        if (rc == 0) {
            /* Shell exited cleanly — this is expected if stdin was empty.
             * Wait a moment then retry. */
            printf("[wlt-init] Shell exited with code 0. Retrying in %d sec...\n", RETRY_DELAY_SEC);
        } else {
            printf("[wlt-init] Shell exited with code %d. Retrying in %d sec...\n", rc, RETRY_DELAY_SEC);
        }
        fflush(stdout);
        sleep(RETRY_DELAY_SEC);
    }

    /* Step 5: Fallback to /bin/sh */
    printf("[wlt-init] WLT-Shell failed %d times. Falling back to /bin/sh.\n", MAX_RETRIES);
    printf("[wlt-init] Type 'help' for available commands.\n");
    fflush(stdout);

    exec_shell(FALLBACK_SHELL);

    /* Step 6: If even /bin/sh fails, halt */
    printf("[wlt-init] FATAL: Cannot start any shell. Halting.\n");
    fflush(stdout);
    sync();
    reboot(LINUX_REBOOT_CMD_POWER_OFF);

    return 0;
}
