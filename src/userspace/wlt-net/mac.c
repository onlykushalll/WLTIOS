/**
 * @file mac.c
 * @brief MAC address randomization
 * @module wlt-net/mac
 * @purpose Generate locally-administered MAC addresses and apply them
 *          via iproute2 (`ip link set address`).
 * @dependencies: net.h
 * @tested_by: tests/userspace/mac_test.c
 * @spec: docs/specs/wlt-net.md
 *
 * Privacy: a fresh MAC is applied on every boot and on every WiFi
 * connect to prevent long-term location tracking by APs.
 */

#include "net.h"
#include "mac.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

/* ─── Generation ─────────────────────────────────────── */

wlt_net_error_t
wlt_mac_generate(uint8_t mac[WLT_MAC_LEN])
{
    if (mac == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    int fd = open(WLT_MAC_URANDOM_PATH, O_RDONLY);
    if (fd < 0)
    {
        WLT_NET_LOG_ERR("open %s: %s", WLT_MAC_URANDOM_PATH,
                         strerror(errno));
        return WLT_NET_ERR_IO;
    }

    ssize_t n = read(fd, mac, WLT_MAC_LEN);
    close(fd);
    if (n != WLT_MAC_LEN)
    {
        WLT_NET_LOG_ERR("short read from urandom: %zd", n);
        return WLT_NET_ERR_IO;
    }

    /* Set locally-administered bit, clear multicast bit.
     * This produces a valid unicast LAA (locally administered address). */
    mac[0] |= WLT_MAC_OUI_LOCALLY_ADMIN;
    mac[0] &= (uint8_t)~WLT_MAC_OUI_MULTICAST;

    return WLT_NET_OK;
}

wlt_net_error_t
wlt_mac_format(const uint8_t mac[WLT_MAC_LEN],
               char *buf, size_t buf_size)
{
    if (mac == NULL || buf == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (buf_size < WLT_MAC_STR_LEN)
        return WLT_NET_ERR_INVALID_ARG;

    int n = snprintf(buf, buf_size,
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2],
                     mac[3], mac[4], mac[5]);
    if (n < 0 || (size_t)n >= buf_size)
        return WLT_NET_ERR_IO;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_mac_parse(const char *str, uint8_t mac[WLT_MAC_LEN])
{
    if (str == NULL || mac == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    /* Accept both xx:xx:xx:xx:xx:xx and xxxxxxxxxxxx formats. */
    unsigned int v[WLT_MAC_LEN];
    int matched = 0;

    if (strchr(str, ':') != NULL)
    {
        matched = sscanf(str, "%x:%x:%x:%x:%x:%x",
                          &v[0], &v[1], &v[2],
                          &v[3], &v[4], &v[5]);
    }
    else
    {
        matched = sscanf(str, "%2x%2x%2x%2x%2x%2x",
                          &v[0], &v[1], &v[2],
                          &v[3], &v[4], &v[5]);
    }

    if (matched != WLT_MAC_LEN)
        return WLT_NET_ERR_PARSE;

    for (int i = 0; i < WLT_MAC_LEN; i++)
    {
        if (v[i] > 0xFF)
            return WLT_NET_ERR_PARSE;
        mac[i] = (uint8_t)v[i];
    }
    return WLT_NET_OK;
}

/* ─── Reading current MAC ────────────────────────────── */

wlt_net_error_t
wlt_mac_get(const char *iface, uint8_t mac[WLT_MAC_LEN])
{
    if (iface == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    /* If caller passes NULL for mac, just probe existence. */
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", iface);

    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        WLT_NET_LOG_ERR("open %s: %s", path, strerror(errno));
        return WLT_NET_ERR_NOT_FOUND;
    }

    char line[WLT_MAC_STR_LEN + 4];
    if (fgets(line, sizeof(line), f) == NULL)
    {
        fclose(f);
        return WLT_NET_ERR_IO;
    }
    fclose(f);

    /* Trim trailing newline. */
    line[strcspn(line, "\r\n")] = '\0';

    if (mac == NULL)
        return WLT_NET_OK;
    return wlt_mac_parse(line, mac);
}

/* ─── Setting MAC ────────────────────────────────────── */

static wlt_net_error_t
ip_link_set(const char *iface, const char *state)
{
    if (iface == NULL || state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    char *argv[] = {
        (char *)"ip",
        (char *)"link",
        (char *)"set",
        (char *)iface,
        (char *)state,
        NULL
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
wlt_mac_set(const char *iface, const uint8_t mac[WLT_MAC_LEN])
{
    if (iface == NULL || mac == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    char mac_str[WLT_MAC_STR_LEN];
    wlt_net_error_t err = wlt_mac_format(mac, mac_str, sizeof(mac_str));
    if (err != WLT_NET_OK)
        return err;

    /* Must bring interface down before changing MAC. */
    err = ip_link_set(iface, "down");
    if (err != WLT_NET_OK)
    {
        WLT_NET_LOG_WRN("ip link set %s down failed: %s",
                         iface, wlt_net_error_str(err));
        /* Continue — may already be down. */
    }

    char *argv[] = {
        (char *)"ip",
        (char *)"link",
        (char *)"set",
        (char *)iface,
        (char *)"address",
        mac_str,
        NULL
    };
    int exit_code = -1;
    err = wlt_net_run_cmd(WLT_NET_IP_CMD, argv, &exit_code);
    if (err != WLT_NET_OK || exit_code != 0)
    {
        WLT_NET_LOG_ERR("set MAC %s on %s failed",
                         mac_str, iface);
        (void)ip_link_set(iface, "up");
        return WLT_NET_ERR_IO;
    }

    err = ip_link_set(iface, "up");
    if (err != WLT_NET_OK)
        WLT_NET_LOG_WRN("ip link set %s up failed: %s",
                         iface, wlt_net_error_str(err));

    WLT_NET_LOG_INF("MAC set: %s = %s", iface, mac_str);
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_mac_randomize(const char *iface)
{
    if (iface == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    uint8_t mac[WLT_MAC_LEN];
    wlt_net_error_t err = wlt_mac_generate(mac);
    if (err != WLT_NET_OK)
        return err;
    return wlt_mac_set(iface, mac);
}
