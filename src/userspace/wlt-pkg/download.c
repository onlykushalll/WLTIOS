/**
 * @file download.c
 * @brief Download packages via Tor SOCKS5 proxy
 * @module wlt-pkg/download
 * @purpose Download .wlt.tar.zst packages through Tor (no IP leak)
 * @dependencies: pkg.h
 * @spec: docs/specs/wlt-pkg.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 */

#include "pkg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define TOR_SOCKS_HOST "127.0.0.1"
#define TOR_SOCKS_PORT 9050
#define DOWNLOAD_BUFFER_SIZE (64 * 1024)
#define MAX_REDIRECTS 5

/* SOCKS5 handshake */
static wlt_pkg_error_t
socks5_connect(int sock, const char *host, uint16_t port)
{
    /* Greeting: version 5, 1 auth method (no auth) */
    uint8_t greeting[] = {0x05, 0x01, 0x00};
    if (send(sock, greeting, sizeof(greeting), 0) < 0)
        return WLT_PKG_ERR_NETWORK;

    uint8_t response[2];
    if (recv(sock, response, sizeof(response), 0) < 2)
        return WLT_PKG_ERR_NETWORK;

    if (response[0] != 0x05 || response[1] != 0x00)
        return WLT_PKG_ERR_NETWORK;

    /* Connect request: version 5, connect, reserved, domain, host, port */
    size_t host_len = strlen(host);
    if (host_len > 255)
        return WLT_PKG_ERR_INVALID_ARG;

    uint8_t request[7 + 256];
    request[0] = 0x05;  /* version */
    request[1] = 0x01;  /* connect */
    request[2] = 0x00;  /* reserved */
    request[3] = 0x03;  /* domain name */
    request[4] = (uint8_t)host_len;
    memcpy(request + 5, host, host_len);
    request[5 + host_len] = (port >> 8) & 0xFF;
    request[6 + host_len] = port & 0xFF;

    if (send(sock, request, 7 + host_len, 0) < 0)
        return WLT_PKG_ERR_NETWORK;

    /* Read response (at least 4 bytes) */
    uint8_t reply[10];
    if (recv(sock, reply, 4, 0) < 4)
        return WLT_PKG_ERR_NETWORK;

    if (reply[1] != 0x00)  /* 0x00 = success */
        return WLT_PKG_ERR_NETWORK;

    /* Read remaining based on address type */
    int extra = 0;
    switch (reply[3])
    {
        case 0x01: extra = 6; break;  /* IPv4: 4 addr + 2 port */
        case 0x03:  /* Domain: 1 len + domain + 2 port */
        {
            uint8_t len;
            if (recv(sock, &len, 1, 0) < 1)
                return WLT_PKG_ERR_NETWORK;
            extra = len + 2;
            break;
        }
        case 0x04: extra = 18; break; /* IPv6: 16 addr + 2 port */
        default: return WLT_PKG_ERR_NETWORK;
    }

    /* Drain remaining bytes */
    uint8_t drain[256];
    while (extra > 0)
    {
        int n = recv(sock, drain, (size_t)extra > sizeof(drain) ? (int)sizeof(drain) : (int)extra, 0);
        if (n <= 0)
            return WLT_PKG_ERR_NETWORK;
        extra -= n;
    }

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_download(const char *url, const char *dest_path)
{
    if (url == NULL || dest_path == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    /* Parse URL: http://host:port/path */
    if (strncmp(url, "http://", 7) != 0)
        return WLT_PKG_ERR_INVALID_ARG;

    const char *host_start = url + 7;
    const char *path_start = strchr(host_start, '/');
    const char *port_sep = strchr(host_start, ':');

    char host[256];
    uint16_t port = 80;
    const char *path = "/";

    if (port_sep != NULL && (path_start == NULL || port_sep < path_start))
    {
        size_t host_len = port_sep - host_start;
        if (host_len >= sizeof(host))
            return WLT_PKG_ERR_INVALID_ARG;
        memcpy(host, host_start, host_len);
        host[host_len] = '\0';
        port = (uint16_t)atoi(port_sep + 1);
    }
    else
    {
        size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
        if (host_len >= sizeof(host))
            return WLT_PKG_ERR_INVALID_ARG;
        memcpy(host, host_start, host_len);
        host[host_len] = '\0';
    }

    if (path_start != NULL)
        path = path_start;

    /* Connect to Tor SOCKS proxy */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return WLT_PKG_ERR_NETWORK;

    struct sockaddr_in proxy_addr;
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(TOR_SOCKS_PORT);
    inet_aton(TOR_SOCKS_HOST, &proxy_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0)
    {
        close(sock);
        return WLT_PKG_ERR_NETWORK;
    }

    /* SOCKS5 connect to target */
    wlt_pkg_error_t err = socks5_connect(sock, host, port);
    if (err != WLT_PKG_OK)
    {
        close(sock);
        return err;
    }

    /* Send HTTP GET */
    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, host);
    if (send(sock, request, strlen(request), 0) < 0)
    {
        close(sock);
        return WLT_PKG_ERR_NETWORK;
    }

    /* Open destination file */
    FILE *f = fopen(dest_path, "wb");
    if (f == NULL)
    {
        close(sock);
        return WLT_PKG_ERR_IO;
    }

    /* Read response + write to file */
    char buffer[DOWNLOAD_BUFFER_SIZE];
    ssize_t n;
    bool header_done = false;
    size_t total_written = 0;

    while ((n = recv(sock, buffer, sizeof(buffer), 0)) > 0)
    {
        if (!header_done)
        {
            /* Find end of HTTP headers (\r\n\r\n) */
            char *header_end = strstr(buffer, "\r\n\r\n");
            if (header_end != NULL)
            {
                header_done = true;
                size_t header_len = header_end - buffer + 4;
                size_t body_len = n - header_len;
                if (body_len > 0)
                {
                    fwrite(buffer + header_len, 1, body_len, f);
                    total_written += body_len;
                }
            }
        }
        else
        {
            fwrite(buffer, 1, n, f);
            total_written += n;
        }
    }

    fclose(f);
    close(sock);

    if (total_written == 0)
    {
        unlink(dest_path);
        return WLT_PKG_ERR_NETWORK;
    }

    return WLT_PKG_OK;
}
