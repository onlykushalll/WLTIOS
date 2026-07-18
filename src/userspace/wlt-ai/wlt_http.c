/**
 * @file wlt_http.c
 * @brief Minimal HTTP/1.1 POST client using raw sockets (replaces libcurl)
 * @purpose ai.c needs HTTP POST to 127.0.0.1:8080 (loopback, no TLS).
 *          This replaces all curl_* calls with a simple socket-based implementation.
 *
 * API mirrors the curl pattern ai.c uses:
 *   wlt_http_post(url, body, content_type, response_buf, response_size, timeout_sec)
 *   returns HTTP status code (200, 404, etc.) or -1 on error
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

/* Parse URL into host, port, path. Returns 0 on success. */
static int parse_url(const char *url, char *host, int hostlen, int *port, char *path, int pathlen) {
    /* Expected format: http://host:port/path or http://host/path */
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    
    /* Extract host (and optional port) */
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');
    
    if (colon && (!slash || colon < slash)) {
        /* host:port */
        int hlen = colon - p;
        if (hlen >= hostlen) hlen = hostlen - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else {
        /* host only, default port 80 */
        int hlen = slash ? (slash - p) : strlen(p);
        if (hlen >= hostlen) hlen = hostlen - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = 80;
    }
    
    /* Extract path */
    if (slash) {
        strncpy(path, slash, pathlen - 1);
        path[pathlen - 1] = '\0';
    } else {
        strcpy(path, "/");
    }
    
    return 0;
}

/* HTTP POST: send body to url, receive response in response_buf.
 * Returns HTTP status code (200, 404, etc.) or -1 on error. */
int wlt_http_post(const char *url, const char *body, const char *content_type,
                  char *response_buf, int response_size, int timeout_sec) {
    char host[256], path[1024];
    int port;
    int sock, ret;
    struct sockaddr_in addr;
    struct hostent *he;
    
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return -1;
    }
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    /* Set timeout */
    struct timeval tv = { .tv_sec = timeout_sec, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    /* Connect */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    /* Try direct IP parse first, then DNS */
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        he = gethostbyname(host);
        if (!he) { close(sock); return -1; }
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
    }
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    /* Build HTTP request */
    int body_len = body ? strlen(body) : 0;
    char request[8192];
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
    
    /* Send request */
    int sent = 0;
    while (sent < req_len) {
        ret = send(sock, request + sent, req_len - sent, 0);
        if (ret <= 0) { close(sock); return -1; }
        sent += ret;
    }
    
    /* Receive response */
    int total = 0;
    while (total < response_size - 1) {
        ret = recv(sock, response_buf + total, response_size - 1 - total, 0);
        if (ret <= 0) break;
        total += ret;
    }
    response_buf[total] = '\0';
    close(sock);
    
    /* Parse HTTP status code from first line */
    int status_code = -1;
    if (total > 12 && strncmp(response_buf, "HTTP/", 5) == 0) {
        status_code = atoi(response_buf + 9); /* "HTTP/1.1 XXX" → skip to XXX */
    }
    
    /* Move body past headers (find \r\n\r\n) */
    char *body_start = strstr(response_buf, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_len_resp = total - (body_start - response_buf);
        memmove(response_buf, body_start, body_len_resp);
        response_buf[body_len_resp] = '\0';
    }
    
    return status_code;
}

/* HTTP GET (for simple health checks) */
int wlt_http_get(const char *url, char *response_buf, int response_size, int timeout_sec) {
    return wlt_http_post(url, NULL, NULL, response_buf, response_size, timeout_sec);
}
