/**
 * @file wlt_socks5.c — SOCKS5 proxy implementation for Tor routing
 */
#include "wlt_socks5.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TOR_SOCKS5_HOST "127.0.0.1"
#define TOR_SOCKS5_PORT 9050
#define SOCKS5_TIMEOUT_MS 15000

/* SOCKS5 method: no authentication (method 0x00) */
#define SOCKS5_VERSION 0x05
#define SOCKS5_METHOD_NO_AUTH 0x00
#define SOCKS5_CMD_CONNECT 0x01
#define SOCKS5_ATYP_DOMAIN 0x03
#define SOCKS5_REPLY_SUCCESS 0x00

static int socks5_send_all(int sock, const uint8_t *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(sock, data + off, len - off, 0);
        if (n <= 0) { if (errno == EINTR) continue; return -1; }
        off += n;
    }
    return 0;
}

static int socks5_recv_exact(int sock, uint8_t *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = recv(sock, buf + off, len - off, 0);
        if (n <= 0) { if (errno == EINTR) continue; return -1; }
        off += n;
    }
    return 0;
}

int wlt_socks5_available(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TOR_SOCKS5_PORT);
    inet_pton(AF_INET, TOR_SOCKS5_HOST, &addr.sin_addr);
    /* Quick connect check — 2s timeout */
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int rc = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);
    return (rc == 0) ? 1 : 0;
}

int wlt_socks5_connect(const char *hostname, int port) {
    if (hostname == NULL || port <= 0) return -1;

    /* Step 1: Connect to Tor's SOCKS5 proxy */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in proxy_addr = {0};
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(TOR_SOCKS5_PORT);
    inet_pton(AF_INET, TOR_SOCKS5_HOST, &proxy_addr.sin_addr);

    struct timeval tv = { .tv_sec = SOCKS5_TIMEOUT_MS / 1000, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        close(sock);
        return -1;
    }

    /* Step 2: SOCKS5 greeting — no authentication */
    uint8_t greeting[3] = { SOCKS5_VERSION, 0x01, SOCKS5_METHOD_NO_AUTH };
    if (socks5_send_all(sock, greeting, 3) < 0) { close(sock); return -1; }

    uint8_t greeting_reply[2];
    if (socks5_recv_exact(sock, greeting_reply, 2) < 0) { close(sock); return -1; }
    if (greeting_reply[0] != SOCKS5_VERSION || greeting_reply[1] != SOCKS5_METHOD_NO_AUTH) {
        close(sock);
        return -1;
    }

    /* Step 3: SOCKS5 connect request — domain name (resolved by Tor, no DNS leak) */
    size_t host_len = strlen(hostname);
    if (host_len > 255) { close(sock); return -1; }

    /* Request: VER CMD RSV ATYP DOMAIN_LEN DOMAIN PORT(2 bytes) */
    size_t req_len = 4 + 1 + host_len + 2;
    uint8_t req[512];  /* enough for 255-char hostname */
    req[0] = SOCKS5_VERSION;
    req[1] = SOCKS5_CMD_CONNECT;
    req[2] = 0x00;  /* reserved */
    req[3] = SOCKS5_ATYP_DOMAIN;
    req[4] = (uint8_t)host_len;
    memcpy(req + 5, hostname, host_len);
    req[5 + host_len] = (uint8_t)(port >> 8);
    req[5 + host_len + 1] = (uint8_t)(port & 0xff);

    if (socks5_send_all(sock, req, req_len) < 0) { close(sock); return -1; }

    /* Step 4: SOCKS5 reply */
    uint8_t reply[10];  /* min 10 bytes: VER REP RSV ATYP + 4 addr + 2 port */
    if (socks5_recv_exact(sock, reply, 10) < 0) { close(sock); return -1; }

    if (reply[0] != SOCKS5_VERSION || reply[1] != SOCKS5_REPLY_SUCCESS) {
        close(sock);
        return -1;
    }

    /* Tunnel established — return the socket for HTTP use */
    return sock;
}
