/**
 * @file ethernet.c
 * @brief Ethernet DHCP/static configuration via iproute2
 * @module wlt-net/ethernet
 * @purpose Bring eth0 up, acquire DHCP lease (or apply static config),
 *          detect cable presence via /sys/class/net/<iface>/carrier.
 * @dependencies: net.h
 * @tested_by: tests/userspace/ethernet_test.c
 * @spec: docs/specs/wlt-net.md
 */

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* ─── Link state ─────────────────────────────────────── */

bool
wlt_ethernet_link_up(const char *iface)
{
    if (iface == NULL)
        return false;

    char path[128];
    snprintf(path, sizeof(path),
             "/sys/class/net/%s/carrier", iface);

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return false;

    int carrier = 0;
    if (fscanf(f, "%d", &carrier) != 1)
    {
        fclose(f);
        return false;
    }
    fclose(f);
    return carrier == 1;
}

wlt_net_error_t
wlt_ethernet_bring_up(const char *iface)
{
    if (iface == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    char *argv[] = {
        (char *)"ip", (char *)"link",
        (char *)"set", (char *)iface, (char *)"up", NULL
    };
    int exit_code = -1;
    wlt_net_error_t err = wlt_net_run_cmd(WLT_NET_IP_CMD, argv, &exit_code);
    if (err != WLT_NET_OK)
        return err;
    if (exit_code != 0)
        return WLT_NET_ERR_IO;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_ethernet_bring_down(const char *iface)
{
    if (iface == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    char *argv[] = {
        (char *)"ip", (char *)"link",
        (char *)"set", (char *)iface, (char *)"down", NULL
    };
    int exit_code = -1;
    wlt_net_error_t err = wlt_net_run_cmd(WLT_NET_IP_CMD, argv, &exit_code);
    if (err != WLT_NET_OK)
        return err;
    if (exit_code != 0)
        return WLT_NET_ERR_IO;
    return WLT_NET_OK;
}

/* ─── Read assigned address back into state ──────────── */

static wlt_net_error_t
read_assigned_address(wlt_net_state_t *state)
{
    char path[160];
    snprintf(path, sizeof(path),
             "/sys/class/net/%s/address", state->eth_iface);

    /* IP address (best-effort): parse /proc/net/fib_trie.
     * Simpler approach: run `ip -o -4 addr show <iface>`. */
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return WLT_NET_ERR_IO;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]); close(pipefd[1]);
        return WLT_NET_ERR_IO;
    }
    if (pid == 0)
    {
        /* Child: route stdout to pipe, exec ip. */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        char *argv[] = {
            (char *)"ip", (char *)"-o", (char *)"-4",
            (char *)"addr", (char *)"show", (char *)state->eth_iface,
            NULL
        };
        execv(WLT_NET_IP_CMD, argv);
        _exit(127);
    }

    close(pipefd[1]);
    char buf[1024];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);
    if (n <= 0)
    {
        (void)waitpid(pid, NULL, 0);
        return WLT_NET_ERR_IO;
    }
    buf[n] = '\0';
    (void)waitpid(pid, NULL, 0);

    /* Output format: "2: eth0    inet 10.0.0.5/24 ..." */
    const char *p = strstr(buf, "inet ");
    if (p != NULL)
    {
        p += 5;
        const char *end = p;
        while (*end && *end != ' ' && *end != '/' && *end != '\n')
            end++;
        size_t len = (size_t)(end - p);
        if (len > 0 && len < WLT_NET_MAX_IP)
        {
            memcpy(state->ip_addr, p, len);
            state->ip_addr[len] = '\0';
        }
    }

    (void)path;  /* address path used implicitly via sysfs elsewhere */
    return WLT_NET_OK;
}

/* ─── DHCP ───────────────────────────────────────────── */

static bool
file_executable(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
            (st.st_mode & 0111) != 0);
}

static wlt_net_error_t
run_dhcp_client(const char *iface)
{
    /* Prefer udhcpc (busybox), fall back to dhclient. */
    const char *cmd = WLT_NET_UDHCPC_CMD;
    if (!file_executable(cmd))
    {
        cmd = WLT_NET_DHCLIENT_CMD;
        if (!file_executable(cmd))
        {
            WLT_NET_LOG_ERR("no DHCP client found");
            return WLT_NET_ERR_NOT_FOUND;
        }
    }

    char *argv_udhcpc[] = {
        (char *)"udhcpc", (char *)"-i", (char *)iface,
        (char *)"-q", (char *)"-t", (char *)"5",
        (char *)"-n", NULL
    };
    char *argv_dhclient[] = {
        (char *)"dhclient", (char *)"-1", (char *)iface, NULL
    };
    char **argv = (strcmp(cmd, WLT_NET_DHCLIENT_CMD) == 0)
                      ? argv_dhclient : argv_udhcpc;

    int exit_code = -1;
    wlt_net_error_t err = wlt_net_run_cmd(cmd, argv, &exit_code);
    if (err != WLT_NET_OK)
        return err;
    if (exit_code != 0)
    {
        WLT_NET_LOG_ERR("DHCP on %s exit %d", iface, exit_code);
        return WLT_NET_ERR_IO;
    }
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_ethernet_configure_dhcp(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    if (!wlt_ethernet_link_up(state->eth_iface))
    {
        WLT_NET_LOG_INF("eth %s: no carrier", state->eth_iface);
        return WLT_NET_ERR_NOT_FOUND;
    }

    wlt_net_error_t err = wlt_ethernet_bring_up(state->eth_iface);
    if (err != WLT_NET_OK)
        return err;

    err = run_dhcp_client(state->eth_iface);
    if (err != WLT_NET_OK)
        return err;

    (void)read_assigned_address(state);
    state->link = WLT_NET_LINK_ETHERNET;
    WLT_NET_LOG_INF("eth %s up via DHCP (ip=%s)",
                     state->eth_iface,
                     state->ip_addr[0] ? state->ip_addr : "?");
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_ethernet_configure_static(wlt_net_state_t *state,
                              const char *ip,
                              const char *gateway,
                              const char *netmask)
{
    if (state == NULL || ip == NULL || gateway == NULL || netmask == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    wlt_net_error_t err = wlt_ethernet_bring_up(state->eth_iface);
    if (err != WLT_NET_OK)
        return err;

    /* Compute prefix length from netmask. */
    int prefix = 0;
    unsigned int a, b, c, d;
    if (sscanf(netmask, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return WLT_NET_ERR_PARSE;
    unsigned int m = (a << 24) | (b << 16) | (c << 8) | d;
    while (m & 0x80000000)
    {
        prefix++;
        m <<= 1;
    }

    char cidr[WLT_NET_MAX_IP];
    snprintf(cidr, sizeof(cidr), "%s/%d", ip, prefix);

    char *argv_addr[] = {
        (char *)"ip", (char *)"addr", (char *)"add",
        cidr, (char *)"dev", (char *)state->eth_iface, NULL
    };
    int exit_code = -1;
    err = wlt_net_run_cmd(WLT_NET_IP_CMD, argv_addr, &exit_code);
    if (err != WLT_NET_OK || exit_code != 0)
    {
        WLT_NET_LOG_ERR("ip addr add failed");
        return WLT_NET_ERR_IO;
    }

    char *argv_gw[] = {
        (char *)"ip", (char *)"route", (char *)"add",
        (char *)"default", (char *)"via", (char *)gateway,
        (char *)"dev", (char *)state->eth_iface, NULL
    };
    exit_code = -1;
    err = wlt_net_run_cmd(WLT_NET_IP_CMD, argv_gw, &exit_code);
    if (err != WLT_NET_OK || exit_code != 0)
        WLT_NET_LOG_WRN("default route add returned %d", exit_code);

    strncpy(state->ip_addr, ip, WLT_NET_MAX_IP - 1);
    state->ip_addr[WLT_NET_MAX_IP - 1] = '\0';
    strncpy(state->gateway, gateway, WLT_NET_MAX_IP - 1);
    state->gateway[WLT_NET_MAX_IP - 1] = '\0';
    state->link = WLT_NET_LINK_ETHERNET;
    WLT_NET_LOG_INF("eth %s up static (ip=%s gw=%s)",
                     state->eth_iface, ip, gateway);
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_ethernet_init(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (state->eth_iface[0] == '\0')
        return WLT_NET_ERR_INVALID_ARG;

    if (!wlt_ethernet_link_up(state->eth_iface))
    {
        WLT_NET_LOG_INF("eth %s: no carrier at boot", state->eth_iface);
        return WLT_NET_OK;
    }

    if (state->eth_auto_config)
        return wlt_ethernet_configure_dhcp(state);

    /* Static config not configured — just bring the link up. */
    return wlt_ethernet_bring_up(state->eth_iface);
}
