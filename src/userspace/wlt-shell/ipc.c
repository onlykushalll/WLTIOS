/**
 * @file ipc.c
 * @brief IPC to WLT-Init + WLT-Compositor
 * @module wlt-shell/ipc
 * @purpose Communicate with init system and compositor
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 */

#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define WLT_SHELL_INIT_SOCKET "/run/wlt-init.sock"
#define WLT_SHELL_COMP_SOCKET "/run/wlt-compositor.sock"
#define WLT_IPC_BUF_SIZE 4096

static int g_init_fd = -1;
static int g_comp_fd = -1;

wlt_shell_error_t
wlt_shell_ipc_init(void)
{
    /* Connect to WLT-Init IPC socket */
    g_init_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_init_fd < 0)
        return WLT_SHELL_ERR_IO;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WLT_SHELL_INIT_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(g_init_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(g_init_fd);
        g_init_fd = -1;
        /* Non-fatal — init might not be running yet */
    }

    /* Connect to WLT-Compositor IPC socket */
    g_comp_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_comp_fd < 0)
        return WLT_SHELL_OK;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WLT_SHELL_COMP_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(g_comp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(g_comp_fd);
        g_comp_fd = -1;
        /* Non-fatal — compositor might not be running yet */
    }

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_ipc_send_init(const char *command, const char *arg)
{
    if (g_init_fd < 0 || command == NULL)
        return WLT_SHELL_ERR_COMPOSITOR;

    char msg[256];
    if (arg != NULL)
        snprintf(msg, sizeof(msg), "%s:%s\n", command, arg);
    else
        snprintf(msg, sizeof(msg), "%s\n", command);

    write(g_init_fd, msg, strlen(msg));

    /* Read response (non-blocking) */
    char buf[WLT_IPC_BUF_SIZE];
    ssize_t n = read(g_init_fd, buf, sizeof(buf) - 1);
    if (n > 0)
    {
        buf[n] = '\0';
        /* Process response */
    }

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_ipc_send_compositor(const char *command)
{
    if (g_comp_fd < 0 || command == NULL)
        return WLT_SHELL_ERR_COMPOSITOR;

    write(g_comp_fd, command, strlen(command));

    return WLT_SHELL_OK;
}

void
wlt_shell_ipc_cleanup(void)
{
    if (g_init_fd >= 0)
    {
        close(g_init_fd);
        g_init_fd = -1;
    }
    if (g_comp_fd >= 0)
    {
        close(g_comp_fd);
        g_comp_fd = -1;
    }
}

int
wlt_shell_ipc_get_init_fd(void)
{
    return g_init_fd;
}

int
wlt_shell_ipc_get_comp_fd(void)
{
    return g_comp_fd;
}
