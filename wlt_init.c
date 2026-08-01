// WLTIOS tiny init replacement (64-bit static ELF)
// Replaces the 32-bit wlt-shell init that can't run on 64-bit kernel.
// Mounts filesystems, runs /etc/init.d/S* scripts, drops to /bin/sh.
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

static void run_script(const char *path) {
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char*)path, "start", NULL};
        execv(path, argv);
        _exit(127);
    }
}

int main(void) {
    // Mount essential filesystems
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);

    // Print boot marker
    const char *marker = getenv("WLT_VM_ID");
    if (!marker) marker = "vm";
    printf("=== WLTIOS %s init started ===\n", marker);
    fflush(stdout);

    // Run all /etc/init.d/S* scripts in background
    DIR *d = opendir("/etc/init.d");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == 'S') {
                char path[256];
                snprintf(path, sizeof(path), "/etc/init.d/%s", e->d_name);
                run_script(path);
            }
        }
        closedir(d);
    }

    // Wait for init scripts to complete (ps-check, tor start)
    sleep(12);

    printf("[%s-init] Dropping to /bin/sh\n", marker);
    fflush(stdout);

    // Drop to shell on console
    int console = open("/dev/console", O_RDWR);
    if (console >= 0) {
        dup2(console, 0);
        dup2(console, 1);
        dup2(console, 2);
    }

    char *argv[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", argv, envp);

    // If exec fails, loop
    for (;;) sleep(1);
    return 0;
}
