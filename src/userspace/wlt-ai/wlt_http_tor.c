/**
 * wlt_http_tor.c — Tor-routed HTTP client (Section D)
 * 
 * Wraps wlt_http_post to route through Tor's SOCKS5 proxy (127.0.0.1:9050).
 * Uses wlt_socks5_connect() (already proven correct in wlt_socks5.c) to
 * establish the tunnel, then sends HTTP over the same socket.
 *
 * This is the same pattern as wlt-pkg/download.c's wlt_pkg_download():
 * raw connect to 9050 → SOCKS5 handshake → HTTP over the tunneled socket.
 */
#include "wlt_socks5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define HTTP_BUF_SIZE 8192
#define RESPONSE_BUF_SIZE 32768

/* Parse a URL into host, port, path. Returns 0 on success. */
static int parse_url(const char *url, char *host, int hostlen, int *port, char *path, int pathlen) {
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        int hlen = colon - p;
        if (hlen >= hostlen) return -1;
        memcpy(host, p, hlen); host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else {
        int hlen = slash ? (int)(slash - p) : (int)strlen(p);
        if (hlen >= hostlen) return -1;
        memcpy(host, p, hlen); host[hlen] = '\0';
        *port = 80;
    }
    if (slash) { strncpy(path, slash, pathlen - 1); path[pathlen - 1] = '\0'; }
    else { strcpy(path, "/"); }
    return 0;
}

/* HTTP GET through Tor SOCKS5. Returns HTTP status code or -1 on error.
 * Routes through 127.0.0.1:9050 — no direct connection, no DNS leak. */
int wlt_http_get_tor(const char *url, char *response, int response_size, int timeout_sec) {
    char host[256], path[1024];
    int port;
    
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0)
        return -1;
    
    /* Check if Tor SOCKS5 is available */
    if (!wlt_socks5_available()) {
        fprintf(stderr, "[http-tor] Tor SOCKS5 not available — refusing (fail-closed)\n");
        return -1;
    }
    
    /* Connect through Tor */
    int sock = wlt_socks5_connect(host, port);
    if (sock < 0) {
        fprintf(stderr, "[http-tor] SOCKS5 connect failed for %s:%d\n", host, port);
        return -1;
    }
    
    /* Set timeouts */
    struct timeval tv = { .tv_sec = timeout_sec > 0 ? timeout_sec : 30, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    /* Build HTTP GET request */
    char request[HTTP_BUF_SIZE];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "User-Agent: WLTIOS-TorBrowser/1.0\r\n"
        "\r\n",
        path, host, port);
    
    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        close(sock); return -1;
    }
    
    /* Send request */
    int sent = 0;
    while (sent < req_len) {
        int n = send(sock, request + sent, req_len - sent, 0);
        if (n <= 0) { close(sock); return -1; }
        sent += n;
    }
    
    /* Receive response */
    int total = 0;
    while (total < response_size - 1) {
        int n = recv(sock, response + total, response_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    close(sock);
    
    /* Parse HTTP status */
    int status = -1;
    if (total > 12 && strncmp(response, "HTTP/", 5) == 0)
        status = atoi(response + 9);
    
    fprintf(stderr, "[http-tor] Fetched %s via Tor: %d bytes, status %d\n", url, total, status);
    return status;
}

/* HTTP POST through Tor SOCKS5. Returns HTTP status code or -1. */
int wlt_http_post_tor(const char *url, const char *body, const char *content_type,
                      char *response, int response_size, int timeout_sec) {
    char host[256], path[1024];
    int port;
    
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0)
        return -1;
    if (!wlt_socks5_available()) return -1;
    
    int sock = wlt_socks5_connect(host, port);
    if (sock < 0) return -1;
    
    struct timeval tv = { .tv_sec = timeout_sec > 0 ? timeout_sec : 30, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    int body_len = body ? (int)strlen(body) : 0;
    char request[HTTP_BUF_SIZE];
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, port, content_type ? content_type : "application/json",
        body_len, body ? body : "");
    
    if (req_len < 0 || req_len >= (int)sizeof(request)) { close(sock); return -1; }
    
    int sent = 0;
    while (sent < req_len) {
        int n = send(sock, request + sent, req_len - sent, 0);
        if (n <= 0) { close(sock); return -1; }
        sent += n;
    }
    
    int total = 0;
    while (total < response_size - 1) {
        int n = recv(sock, response + total, response_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    close(sock);
    
    int status = -1;
    if (total > 12 && strncmp(response, "HTTP/", 5) == 0)
        status = atoi(response + 9);
    return status;
}
