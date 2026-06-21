/**
 * @file shutdown.c
 * @brief Shutdown + reboot logic for WLT-Init
 * @module wlt-init/shutdown
 * @purpose Clean shutdown sequence — stop services, unmount, power off
 * @dependencies: init.h, service.h, mount.h, log.h
 * @tested_by: tests/userspace/shutdown_test.c
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 */

#include "init.h"
#include "service.h"
#include "mount.h"
#include "log.h"
#include <unistd.h>
#include <sys/reboot.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

/* Linux reboot flags */
#ifndef LINUX_REBOOT_CMD_POWER_OFF
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
#endif
#ifndef LINUX_REBOOT_CMD_RESTART
#define LINUX_REBOOT_CMD_RESTART    0x01234567
#endif

/**
 * Shutdown the system cleanly
 * @param init  Init state (to stop services)
 * @param reboot  true = reboot, false = power off
 */
void
wlt_shutdown(wlt_init_t *init, bool reboot)
{
    WLT_LOG_INF("Shutdown initiated (reboot=%s)", reboot ? "yes" : "no");

    /* 1. Stop all services in reverse order */
    WLT_LOG_INF("Stopping all services...");
    for (int i = (int)init->service_count - 1; i >= 0; i--)
    {
        wlt_service_t *svc = &init->services[i];
        if (svc->state == WLT_SVC_RUNNING || svc->state == WLT_SVC_FROZEN)
        {
            WLT_LOG_INF("Stopping %s...", svc->name);
            wlt_service_stop(init, svc);
        }
    }

    /* 2. Wait for all processes to exit */
    WLT_LOG_INF("Waiting for processes to exit...");
    sync();  /* Flush filesystem buffers */

    /* 3. Unmount filesystems */
    WLT_LOG_INF("Unmounting filesystems...");
    wlt_mount_unmount_all();

    /* 4. Final sync */
    sync();
    WLT_LOG_INF("Filesystems synced");

    /* 5. Remount root read-only */
    wlt_mount_root_readonly();

    /* 6. Power off or reboot */
    WLT_LOG_INF("Power %s...", reboot ? "cycle" : "off");
    wlt_log_close();

    if (reboot)
    {
        reboot(LINUX_REBOOT_CMD_RESTART);
    }
    else
    {
        reboot(LINUX_REBOOT_CMD_POWER_OFF);
    }

    /* If we reach here, reboot() failed */
    WLT_LOG_ERR("reboot() syscall failed: %s", strerror(errno));

    /* Last resort: kernel panic */
    /* This should never happen */
    while (1)
    {
        pause();
    }
}

/**
 * Handle SIGTERM (init receives this for shutdown)
 */
void
wlt_shutdown_signal_handler(int sig)
{
    (void)sig;
    /* Set a flag that the monitor loop checks */
    /* The actual shutdown happens in the main loop */
    /* TODO(wlt-init): Implement via signal-safe flag */
}
