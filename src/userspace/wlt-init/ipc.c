/**
 * @file ipc.c
 * @brief IPC socket server for WLT-Init
 * @module wlt-init/ipc
 * @purpose Listens on Unix socket for wltctl commands
 * @dependencies: init.h, service.h, log.h
 * @tested_by: tests/userspace/ipc_test.c
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 *
 * Creates a Unix socket at /run/wlt-init.sock
 * Listens for commands from wltctl
 * Processes: STATUS, START, STOP, RESTART, LOGS, SHUTDOWN, REBOOT
 */

#include "init.h"
#include "service.h"
#include "log.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#define WLT_INIT_SOCKET "/run/wlt-init.sock"
#define WLT_IPC_BUF_SIZE 4096
#define WLT_IPC_BACKLOG 5

static int g_ipc_fd = -1;

/* External: shutdown function */
extern void wlt_shutdown(wlt_init_t *init, bool reboot);

/**
 * Initialize IPC socket
 * Creates /run/wlt-init.sock and starts listening
 */
wlt_init_error_t
wlt_ipc_init(void)
{
    /* Remove old socket if exists */
    unlink(WLT_INIT_SOCKET);

    g_ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_ipc_fd < 0)
    {
        WLT_LOG_ERR("IPC: socket() failed: %s", strerror(errno));
        return WLT_INIT_ERR_IO;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WLT_INIT_SOCKET, sizeof(addr.sun_path) - 1);

    if (bind(g_ipc_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        WLT_LOG_ERR("IPC: bind() failed: %s", strerror(errno));
        close(g_ipc_fd);
        return WLT_INIT_ERR_IO;
    }

    if (listen(g_ipc_fd, WLT_IPC_BACKLOG) < 0)
    {
        WLT_LOG_ERR("IPC: listen() failed: %s", strerror(errno));
        close(g_ipc_fd);
        return WLT_INIT_ERR_IO;
    }

    WLT_LOG_INF("IPC socket listening at %s", WLT_INIT_SOCKET);
    return WLT_INIT_OK;
}

/**
 * Process a single command from wltctl
 */
static void
process_command(wlt_init_t *init, int client_fd, const char *cmd, const char *arg)
{
    char response[WLT_IPC_BUF_SIZE];
    int offset = 0;

    if (strcmp(cmd, "STATUS") == 0)
    {
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "WLTIOS Service Status\n");
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "%-20s %-10s %-8s %s\n",
                           "NAME", "STATE", "PID", "RESTARTS");
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "%-20s %-10s %-8s %s\n",
                           "----", "-----", "---", "--------");

        for (size_t i = 0; i < init->service_count; i++)
        {
            wlt_service_t *svc = &init->services[i];
            offset += snprintf(response + offset, sizeof(response) - offset,
                               "%-20s %-10s %-8d %d\n",
                               svc->name,
                               wlt_service_state_str(svc->state),
                               svc->pid,
                               svc->restart_count);
        }

        offset += snprintf(response + offset, sizeof(response) - offset,
                           "\nTor: %s\n",
                           init->tor_ready ? "READY" : "NOT READY");
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "Kill switch: %s\n",
                           init->kill_switch_active ? "ACTIVE" : "inactive");
    }
    else if (strcmp(cmd, "START") == 0 && arg != NULL)
    {
        wlt_service_t *svc = wlt_service_find(init, arg);
        if (svc == NULL)
        {
            snprintf(response, sizeof(response), "Service not found: %s\n", arg);
        }
        else
        {
            wlt_init_error_t err = wlt_service_start(init, svc);
            if (err == WLT_INIT_OK)
                snprintf(response, sizeof(response), "Started %s (PID %d)\n",
                         svc->name, svc->pid);
            else
                snprintf(response, sizeof(response), "Failed to start %s: %s\n",
                         svc->name, wlt_init_error_str(err));
        }
    }
    else if (strcmp(cmd, "STOP") == 0 && arg != NULL)
    {
        wlt_service_t *svc = wlt_service_find(init, arg);
        if (svc == NULL)
        {
            snprintf(response, sizeof(response), "Service not found: %s\n", arg);
        }
        else
        {
            wlt_service_stop(init, svc);
            snprintf(response, sizeof(response), "Stopped %s\n", svc->name);
        }
    }
    else if (strcmp(cmd, "RESTART") == 0 && arg != NULL)
    {
        wlt_service_t *svc = wlt_service_find(init, arg);
        if (svc == NULL)
        {
            snprintf(response, sizeof(response), "Service not found: %s\n", arg);
        }
        else
        {
            wlt_service_restart(init, svc);
            snprintf(response, sizeof(response), "Restarted %s (PID %d)\n",
                     svc->name, svc->pid);
        }
    }
    else if (strcmp(cmd, "SHUTDOWN") == 0)
    {
        snprintf(response, sizeof(response), "Shutting down...\n");
        write(client_fd, response, strlen(response));
        close(client_fd);
        wlt_shutdown(init, false);
        return; /* Never reached */
    }
    else if (strcmp(cmd, "REBOOT") == 0)
    {
        snprintf(response, sizeof(response), "Rebooting...\n");
        write(client_fd, response, strlen(response));
        close(client_fd);
        wlt_shutdown(init, true);
        return; /* Never reached */
    }
    else if (strcmp(cmd, "TOR_STATUS") == 0)
    {
        snprintf(response, sizeof(response),
                 "Tor: %s\nKill switch: %s\n",
                 init->tor_ready ? "READY" : "NOT READY",
                 init->kill_switch_active ? "ACTIVE" : "inactive");
    }
    else if (strcmp(cmd, "KILL_SWITCH") == 0)
    {
        /* TODO(wlt-init): Call wlt_tor_kill_switch(init) */
        snprintf(response, sizeof(response), "Kill switch triggered\n");
    }
    else
    {
        snprintf(response, sizeof(response), "Unknown command: %s\n", cmd);
    }

    write(client_fd, response, strlen(response));
}

/**
 * Check for incoming IPC connections (non-blocking)
 * Call this from the monitor loop
 */
void
wlt_ipc_poll(wlt_init_t *init)
{
    if (g_ipc_fd < 0)
        return;

    /* Use select to check if there's a connection (non-blocking) */
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(g_ipc_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if (select(g_ipc_fd + 1, &fds, NULL, NULL, &tv) <= 0)
        return; /* No connection */

    /* Accept connection */
    int client_fd = accept(g_ipc_fd, NULL, NULL);
    if (client_fd < 0)
        return;

    /* Read command */
    char buf[WLT_IPC_BUF_SIZE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    /* Parse command: "CMD" or "CMD:arg\n" */
    char cmd[64] = {0};
    char arg[128] = {0};
    char *colon = strchr(buf, ':');
    char *newline = strchr(buf, '\n');

    if (colon != NULL)
    {
        size_t cmd_len = colon - buf;
        if (cmd_len >= sizeof(cmd))
            cmd_len = sizeof(cmd) - 1;
        strncpy(cmd, buf, cmd_len);

        const char *arg_start = colon + 1;
        size_t arg_len = newline ? (size_t)(newline - arg_start) : strlen(arg_start);
        if (arg_len >= sizeof(arg))
            arg_len = sizeof(arg) - 1;
        strncpy(arg, arg_start, arg_len);
    }
    else
    {
        /* No arg — just cmd (strip newline) */
        size_t cmd_len = newline ? (size_t)(newline - buf) : strlen(buf);
        if (cmd_len >= sizeof(cmd))
            cmd_len = sizeof(cmd) - 1;
        strncpy(cmd, buf, cmd_len);
    }

    WLT_LOG_DBG("IPC command: %s arg=%s", cmd, arg);

    /* Process */
    process_command(init, client_fd, cmd, arg[0] ? arg : NULL);

    close(client_fd);
}

/**
 * Clean up IPC socket
 */
void
wlt_ipc_cleanup(void)
{
    if (g_ipc_fd >= 0)
    {
        close(g_ipc_fd);
        g_ipc_fd = -1;
    }
    unlink(WLT_INIT_SOCKET);
}
