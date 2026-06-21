/**
 * @file ctl.c
 * @brief wltctl command-line tool for controlling WLT-Init
 * @module wlt-init/ctl
 * @purpose Allows user to query service status, start/stop services, etc.
 * @dependencies: init.h, service.h, log.h
 * @tested_by: tests/userspace/ctl_test.c
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 *
 * Usage:
 *   wltctl status              # Show all services
 *   wltctl start <name>        # Start a service
 *   wltctl stop <name>         # Stop a service
 *   wltctl restart <name>      # Restart a service
 *   wltctl logs <name>         # Show recent logs
 *   wltctl tor-status          # Show Tor status
 *   wltctl kill-switch         # Trigger kill switch
 *   wltctl shutdown            # Clean shutdown
 *   wltctl reboot              # Reboot
 *
 * Communication: wltctl talks to WLT-Init via Unix socket (/run/wlt-init.sock)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define WLT_INIT_SOCKET "/run/wlt-init.sock"
#define WLT_CTL_BUF_SIZE 4096

typedef enum {
    WLT_CTL_CMD_STATUS = 0,
    WLT_CTL_CMD_START,
    WLT_CTL_CMD_STOP,
    WLT_CTL_CMD_RESTART,
    WLT_CTL_CMD_LOGS,
    WLT_CTL_CMD_TOR_STATUS,
    WLT_CTL_CMD_KILL_SWITCH,
    WLT_CTL_CMD_SHUTDOWN,
    WLT_CTL_CMD_REBOOT,
    WLT_CTL_CMD_ENABLE,
    WLT_CTL_CMD_DISABLE,
} wlt_ctl_cmd_t;

static void
print_usage(void)
{
    printf("WLTIOS Control Tool (wltctl)\n");
    printf("Usage: wltctl <command> [args]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  status              Show all services\n");
    printf("  start <name>        Start a service\n");
    printf("  stop <name>         Stop a service\n");
    printf("  restart <name>      Restart a service\n");
    printf("  logs <name>         Show recent logs for a service\n");
    printf("  tor-status          Show Tor circuit status\n");
    printf("  kill-switch         Manually trigger kill switch\n");
    printf("  shutdown            Clean shutdown\n");
    printf("  reboot              Reboot the system\n");
    printf("  enable <name>       Enable service at boot\n");
    printf("  disable <name>      Disable service at boot\n");
    printf("\n");
}

static int
connect_to_init(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WLT_INIT_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "Cannot connect to WLT-Init (%s): %s\n",
                WLT_INIT_SOCKET, strerror(errno));
        fprintf(stderr, "Is WLT-Init running?\n");
        close(fd);
        return -1;
    }

    return fd;
}

static int
send_command(wlt_ctl_cmd_t cmd, const char *arg)
{
    int fd = connect_to_init();
    if (fd < 0)
        return 1;

    /* Build command message: "CMD:arg\n" */
    char msg[256];
    const char *cmd_str[] = {
        "STATUS", "START", "STOP", "RESTART", "LOGS",
        "TOR_STATUS", "KILL_SWITCH", "SHUTDOWN", "REBOOT",
        "ENABLE", "DISABLE"
    };

    if (arg != NULL)
        snprintf(msg, sizeof(msg), "%s:%s\n", cmd_str[cmd], arg);
    else
        snprintf(msg, sizeof(msg), "%s\n", cmd_str[cmd]);

    /* Send command */
    write(fd, msg, strlen(msg));

    /* Read response */
    char buf[WLT_CTL_BUF_SIZE];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        printf("%s", buf);
    }

    close(fd);
    return 0;
}

int
main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        return send_command(WLT_CTL_CMD_STATUS, NULL);
    }
    else if (strcmp(argv[1], "start") == 0 && argc > 2)
    {
        return send_command(WLT_CTL_CMD_START, argv[2]);
    }
    else if (strcmp(argv[1], "stop") == 0 && argc > 2)
    {
        return send_command(WLT_CTL_CMD_STOP, argv[2]);
    }
    else if (strcmp(argv[1], "restart") == 0 && argc > 2)
    {
        return send_command(WLT_CTL_CMD_RESTART, argv[2]);
    }
    else if (strcmp(argv[1], "logs") == 0 && argc > 2)
    {
        return send_command(WLT_CTL_CMD_LOGS, argv[2]);
    }
    else if (strcmp(argv[1], "tor-status") == 0)
    {
        return send_command(WLT_CTL_CMD_TOR_STATUS, NULL);
    }
    else if (strcmp(argv[1], "kill-switch") == 0)
    {
        return send_command(WLT_CTL_CMD_KILL_SWITCH, NULL);
    }
    else if (strcmp(argv[1], "shutdown") == 0)
    {
        printf("Initiating clean shutdown...\n");
        return send_command(WLT_CTL_CMD_SHUTDOWN, NULL);
    }
    else if (strcmp(argv[1], "reboot") == 0)
    {
        printf("Rebooting...\n");
        return send_command(WLT_CTL_CMD_REBOOT, NULL);
    }
    else if (strcmp(argv[1], "enable") == 0 && argc > 2)
    {
        return send_command(WLT_CTL_CMD_ENABLE, argv[2]);
    }
    else if (strcmp(argv[1], "disable") == 0 && argc > 2)
    {
        return send_command(WLT_CTL_CMD_DISABLE, argv[2]);
    }
    else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0)
    {
        print_usage();
        return 0;
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }
}
