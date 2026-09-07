/**
 * @file wifi.c
 * @brief WiFi scanning and connection
 * @module wlt-net/wifi
 * @purpose Drive wpa_supplicant via its UNIX-domain control socket
 *          (SCAN, SCAN_RESULTS, ADD_NETWORK, ENABLE_NETWORK).
 * @dependencies: net.h, mac.h
 * @tested_by: tests/userspace/wifi_test.c
 * @spec: docs/specs/wlt-net.md
 *
 * Why wpa_supplicant rather than raw nl80211: wpa_supplicant already
 * handles the full WPA handshake and key negotiation. We drive it via
 * its text control protocol over a DGRAM socket — no D-Bus dependency.
 */

#include "net.h"
#include "wifi.h"
#include "mac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>

/* ─── Hardware detection ─────────────────────────────── */

bool
wlt_wifi_hardware_present(void)
{
    DIR *dir = opendir("/sys/class/net");
    if (dir == NULL)
        return false;

    struct dirent *e;
    bool found = false;
    while ((e = readdir(dir)) != NULL)
    {
        if (e->d_name[0] == '.')
            continue;
        char path[320];
        snprintf(path, sizeof(path),
                 "/sys/class/net/%s/wireless", e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        {
            found = true;
            break;
        }
    }
    closedir(dir);
    return found;
}

wlt_net_error_t
wlt_wifi_find_iface(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return WLT_NET_ERR_INVALID_ARG;

    DIR *dir = opendir("/sys/class/net");
    if (dir == NULL)
        return WLT_NET_ERR_IO;

    struct dirent *e;
    wlt_net_error_t err = WLT_NET_ERR_NOT_FOUND;
    while ((e = readdir(dir)) != NULL)
    {
        if (e->d_name[0] == '.')
            continue;
        char path[320];
        snprintf(path, sizeof(path),
                 "/sys/class/net/%s/wireless", e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        {
            snprintf(buf, buf_size, "%s", e->d_name);
            err = WLT_NET_OK;
            break;
        }
    }
    closedir(dir);
    return err;
}

/* ─── wpa_supplicant control socket ──────────────────── */

static wlt_net_error_t
wpa_open(const char *iface, int *out_fd, char *local_path, size_t path_size)
{
    if (iface == NULL || out_fd == NULL || local_path == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        WLT_NET_LOG_ERR("socket(): %s", strerror(errno));
        return WLT_NET_ERR_IO;
    }

    /* Bind to a unique local path so wpa_supplicant can reply. */
    snprintf(local_path, path_size, "/tmp/wlt-net-wpa-%d", (int)getpid());
    unlink(local_path);

    struct sockaddr_un local;
    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    snprintf(local.sun_path, sizeof(local.sun_path), "%s", local_path);

    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        WLT_NET_LOG_ERR("bind(): %s", strerror(errno));
        close(fd);
        return WLT_NET_ERR_IO;
    }

    struct sockaddr_un remote;
    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    snprintf(remote.sun_path, sizeof(remote.sun_path),
             "%s/%s", WLT_WIFI_WPA_CTRL_DIR, iface);

    if (connect(fd, (struct sockaddr *)&remote, sizeof(remote)) < 0)
    {
        WLT_NET_LOG_ERR("connect %s: %s", remote.sun_path, strerror(errno));
        close(fd);
        unlink(local_path);
        return WLT_NET_ERR_NOT_FOUND;
    }
    *out_fd = fd;
    return WLT_NET_OK;
}

static wlt_net_error_t
wpa_request(int fd, const char *cmd, char *resp, size_t resp_size)
{
    if (cmd == NULL || resp == NULL || resp_size == 0)
        return WLT_NET_ERR_INVALID_ARG;

    size_t cmd_len = strlen(cmd);
    if (write(fd, cmd, cmd_len) != (ssize_t)cmd_len)
    {
        WLT_NET_LOG_ERR("wpa write '%s': %s", cmd, strerror(errno));
        return WLT_NET_ERR_IO;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
    int pr = poll(&pfd, 1, WLT_WIFI_WPA_TIMEOUT_MS);
    if (pr <= 0)
    {
        WLT_NET_LOG_ERR("wpa poll '%s' timeout", cmd);
        return WLT_NET_ERR_TIMEOUT;
    }

    ssize_t n = read(fd, resp, resp_size - 1);
    if (n < 0)
    {
        WLT_NET_LOG_ERR("wpa read: %s", strerror(errno));
        return WLT_NET_ERR_IO;
    }
    resp[n] = '\0';
    return WLT_NET_OK;
}

/* ─── Scan result parsing ────────────────────────────── */

static void
parse_scan_line(char *line, wlt_net_wifi_network_t *net)
{
    /* Format: bssid \t freq \t signal \t flags \t ssid */
    char *save = NULL;
    char *bssid_s = strtok_r(line, "\t", &save);
    char *freq_s  = strtok_r(NULL, "\t", &save);
    char *sig_s   = strtok_r(NULL, "\t", &save);
    char *flags_s = strtok_r(NULL, "\t", &save);
    char *ssid_s  = strtok_r(NULL, "\t", &save);

    memset(net, 0, sizeof(*net));

    if (bssid_s != NULL)
        (void)wlt_mac_parse(bssid_s, net->bssid);

    int freq = 0;
    if (freq_s != NULL)
        freq = atoi(freq_s);
    /* Convert frequency (MHz) to a rough channel number. */
    if (freq >= 2412 && freq <= 2472)
        net->channel = (uint16_t)((freq - 2407) / 5);
    else if (freq == 2484)
        net->channel = 14;
    else if (freq >= 5180 && freq <= 5945)
        net->channel = (uint16_t)((freq - 5000) / 5);
    else
        net->channel = 0;

    if (sig_s != NULL)
        net->signal_dbm = (int8_t)atoi(sig_s);

    net->secure = (flags_s != NULL && strstr(flags_s, "WPA") != NULL);

    if (ssid_s != NULL && ssid_s[0] != '\0')
    {
        strncpy(net->ssid, ssid_s, WLT_NET_MAX_SSID - 1);
        net->ssid[WLT_NET_MAX_SSID - 1] = '\0';
    }
    else
    {
        strncpy(net->ssid, "<hidden>", WLT_NET_MAX_SSID - 1);
    }
}

static wlt_net_error_t
parse_scan_results(wlt_net_state_t *state, const char *resp)
{
    state->scan_count = 0;

    const char *line = resp;
    /* Skip header line. */
    while (*line && *line != '\n')
        line++;
    if (*line == '\n')
        line++;

    while (*line && state->scan_count < WLT_WIFI_MAX_NETWORKS)
    {
        const char *eol = strchr(line, '\n');
        size_t len = (eol != NULL) ? (size_t)(eol - line) : strlen(line);
        if (len == 0)
            break;

        char buf[WLT_NET_MAX_LINE];
        if (len >= sizeof(buf))
            len = sizeof(buf) - 1;
        memcpy(buf, line, len);
        buf[len] = '\0';

        parse_scan_line(buf, &state->scan_results[state->scan_count]);
        state->scan_count++;

        if (eol == NULL)
            break;
        line = eol + 1;
    }
    return WLT_NET_OK;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_net_error_t
wlt_wifi_init(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    /* If config provided a placeholder, try to find the real interface. */
    if (state->iface[0] == '\0' ||
        strcmp(state->iface, "wlan0") == 0)
    {
        char real[WLT_NET_MAX_IFACE];
        if (wlt_wifi_find_iface(real, sizeof(real)) == WLT_NET_OK)
        {
            strncpy(state->iface, real, WLT_NET_MAX_IFACE - 1);
            state->iface[WLT_NET_MAX_IFACE - 1] = '\0';
        }
    }

    /* sun_path is 108 bytes on Linux — keep our local path well under
     * that to avoid truncation warnings from the compiler. */
    char local_path[64];
    int fd = -1;
    wlt_net_error_t err = wpa_open(state->iface, &fd,
                                     local_path, sizeof(local_path));
    if (err != WLT_NET_OK)
    {
        WLT_NET_LOG_WRN("wpa_supplicant not reachable on %s — "
                         "WiFi will be unavailable", state->iface);
        state->wpa_ctrl_fd = -1;
        return err;
    }
    state->wpa_ctrl_fd = fd;
    /* Cleanup uses pid-derived path: /tmp/wlt-net-wpa-<pid>. */

    WLT_NET_LOG_INF("wpa_supplicant attached on %s", state->iface);
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_wifi_scan(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (state->wpa_ctrl_fd < 0)
        return WLT_NET_ERR_NOT_FOUND;

    char resp[WLT_WIFI_WPA_RESP_BUF];

    for (int attempt = 0; attempt < WLT_NET_SCAN_RETRIES; attempt++)
    {
        wlt_net_error_t err = wpa_request(state->wpa_ctrl_fd,
                                           "SCAN", resp, sizeof(resp));
        if (err == WLT_NET_OK && strncmp(resp, "OK", 2) == 0)
        {
            /* Poll SCAN_RESULTS until non-empty or timeout. wpa_supplicant
             * echoes "FAIL-BUSY" if a scan is already in progress. */
            for (int wait = 0; wait < WLT_NET_SCAN_TIMEOUT_SEC * 4; wait++)
            {
                usleep(250000);  /* 250ms */
                err = wpa_request(state->wpa_ctrl_fd, "SCAN_RESULTS",
                                   resp, sizeof(resp));
                if (err != WLT_NET_OK)
                    continue;
                const char *nl = strchr(resp, '\n');
                if (nl != NULL && nl[1] != '\0')
                    return parse_scan_results(state, resp);
            }
            WLT_NET_LOG_WRN("scan timed out (attempt %d)", attempt + 1);
            continue;
        }
        if (err == WLT_NET_OK && strstr(resp, "FAIL-BUSY") != NULL)
        {
            WLT_NET_LOG_DBG("scan busy (attempt %d)", attempt + 1);
            usleep(500000);
            continue;
        }
        WLT_NET_LOG_WRN("SCAN attempt %d failed", attempt + 1);
    }

    /* No networks found — empty list, not an error per spec. */
    state->scan_count = 0;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_wifi_connect(wlt_net_state_t *state,
                 const char *ssid, const char *psk)
{
    if (state == NULL || ssid == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (state->wpa_ctrl_fd < 0)
        return WLT_NET_ERR_NOT_FOUND;
    /* [fix] ssid/psk get embedded unescaped into a quoted
     * wpa_supplicant SET_NETWORK control command below. WPA-PSK
     * passwords and SSIDs can legitimately contain '"'. Rather than
     * hand-rolling escaping that has to exactly match wpa_supplicant's
     * own parser (risky to get right without a live reference), reject
     * any value containing a quote outright - fail closed on
     * ambiguous input, same as the rest of this codebase does.
     * [fix 2026-08-16] Same reasoning extends to backslash: wpa_config's
     * string parser treats '\' as an escape introducer (used for
     * hex/control-char encoding of raw SSIDs in its .conf grammar), so a
     * trailing or otherwise placed '\' in an unescaped value risks
     * consuming the closing quote or altering what gets stored - the
     * same class of ambiguity the quote check above already exists to
     * avoid. Rejecting it outright is the same fail-closed call. */
    if (strchr(ssid, '"') != NULL || strchr(ssid, '\\') != NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (psk != NULL &&
        (strchr(psk, '"') != NULL || strchr(psk, '\\') != NULL))
        return WLT_NET_ERR_INVALID_ARG;

    /* Randomize MAC before each connect (privacy). */
    if (state->mac_mode == WLT_NET_MAC_ALWAYS)
        (void)wlt_mac_randomize(state->iface);

    char resp[WLT_WIFI_WPA_RESP_BUF];
    char cmd[WLT_NET_MAX_LINE];
    wlt_net_error_t err;

    /* 1. Add a new network block. */
    err = wpa_request(state->wpa_ctrl_fd, "ADD_NETWORK",
                       resp, sizeof(resp));
    if (err != WLT_NET_OK || atoi(resp) < 0)
    {
        WLT_NET_LOG_ERR("ADD_NETWORK failed: %s", resp);
        return WLT_NET_ERR_WIFI_CONNECT;
    }
    int net_id = atoi(resp);

    /* 2. Set SSID. wpa_supplicant expects the value quoted. */
    snprintf(cmd, sizeof(cmd), "SET_NETWORK %d ssid \"%s\"", net_id, ssid);
    err = wpa_request(state->wpa_ctrl_fd, cmd, resp, sizeof(resp));
    if (err != WLT_NET_OK || strncmp(resp, "OK", 2) != 0)
    {
        snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", net_id);
        (void)wpa_request(state->wpa_ctrl_fd, cmd, resp, sizeof(resp));
        WLT_NET_LOG_ERR("wifi connect to %s failed", ssid);
        return WLT_NET_ERR_WIFI_CONNECT;
    }

    /* 3. Set PSK (if provided) — otherwise mark as open. */
    if (psk != NULL && psk[0] != '\0')
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d psk \"%s\"", net_id, psk);
    else
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d key_mgmt NONE", net_id);
    err = wpa_request(state->wpa_ctrl_fd, cmd, resp, sizeof(resp));
    if (err != WLT_NET_OK || strncmp(resp, "OK", 2) != 0)
    {
        snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", net_id);
        (void)wpa_request(state->wpa_ctrl_fd, cmd, resp, sizeof(resp));
        WLT_NET_LOG_ERR("wifi connect to %s failed", ssid);
        return WLT_NET_ERR_WIFI_CONNECT;
    }

    /* 4. Enable + 5. Save config so it survives supplicant restart. */
    snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %d", net_id);
    err = wpa_request(state->wpa_ctrl_fd, cmd, resp, sizeof(resp));
    if (err != WLT_NET_OK || strncmp(resp, "OK", 2) != 0)
    {
        snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", net_id);
        (void)wpa_request(state->wpa_ctrl_fd, cmd, resp, sizeof(resp));
        WLT_NET_LOG_ERR("wifi connect to %s failed", ssid);
        return WLT_NET_ERR_WIFI_CONNECT;
    }
    (void)wpa_request(state->wpa_ctrl_fd, "SAVE_CONFIG",
                       resp, sizeof(resp));

    /* 6. Update state. */
    snprintf(state->connected_ssid, WLT_NET_MAX_SSID, "%s", ssid);
    state->link = WLT_NET_LINK_WIFI;
    WLT_NET_LOG_INF("WiFi connected to %s (net_id=%d)", ssid, net_id);
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_wifi_disconnect(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (state->wpa_ctrl_fd < 0)
        return WLT_NET_OK;

    char resp[WLT_WIFI_WPA_RESP_BUF];
    /* DISCONNECT tears down the current association. */
    wlt_net_error_t err = wpa_request(state->wpa_ctrl_fd,
                                       "DISCONNECT", resp, sizeof(resp));
    if (err != WLT_NET_OK || strncmp(resp, "OK", 2) != 0)
        WLT_NET_LOG_WRN("DISCONNECT returned: %s", resp);

    state->connected_ssid[0] = '\0';
    if (state->link == WLT_NET_LINK_WIFI)
        state->link = WLT_NET_LINK_DOWN;
    WLT_NET_LOG_INF("WiFi disconnected");
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_wifi_forget(wlt_net_state_t *state, const char *ssid)
{
    if (state == NULL || ssid == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (state->wpa_ctrl_fd < 0)
        return WLT_NET_ERR_NOT_FOUND;

    char resp[WLT_WIFI_WPA_RESP_BUF];
    char cmd[WLT_NET_MAX_LINE];

    /* LIST_NETWORKS returns: network_id \t ssid \t bssid \t flags */
    wlt_net_error_t err = wpa_request(state->wpa_ctrl_fd,
                                       "LIST_NETWORKS",
                                       resp, sizeof(resp));
    if (err != WLT_NET_OK)
        return err;

    /* Walk lines, match SSID, remove by id. */
    const char *line = resp;
    while (*line)
    {
        const char *eol = strchr(line, '\n');
        size_t len = (eol != NULL) ? (size_t)(eol - line) : strlen(line);
        char buf[WLT_NET_MAX_LINE];
        if (len >= sizeof(buf))
            len = sizeof(buf) - 1;
        memcpy(buf, line, len);
        buf[len] = '\0';

        int net_id = -1;
        char ssid_field[WLT_NET_MAX_SSID] = "";
        if (sscanf(buf, "%d\t%63[^\t]", &net_id, ssid_field) >= 1)
        {
            if (net_id >= 0 && strcmp(ssid_field, ssid) == 0)
            {
                snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", net_id);
                (void)wpa_request(state->wpa_ctrl_fd, cmd,
                                   resp, sizeof(resp));
                (void)wpa_request(state->wpa_ctrl_fd, "SAVE_CONFIG",
                                   resp, sizeof(resp));
                WLT_NET_LOG_INF("Forgot WiFi: %s", ssid);
                return WLT_NET_OK;
            }
        }
        if (eol == NULL)
            break;
        line = eol + 1;
    }
    return WLT_NET_ERR_NOT_FOUND;
}

wlt_net_error_t
wlt_wifi_cleanup(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    if (state->wpa_ctrl_fd >= 0)
    {
        close(state->wpa_ctrl_fd);
        state->wpa_ctrl_fd = -1;
        char path[128];
        snprintf(path, sizeof(path),
                 "/tmp/wlt-net-wpa-%d", (int)getpid());
        unlink(path);
    }
    return WLT_NET_OK;
}
