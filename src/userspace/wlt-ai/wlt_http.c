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
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
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
 * Returns HTTP status code (200, 404, etc.) or -1 on error.
 *
 * Robustness notes (audit B-C1/C2/C3, E-C4/H4):
 *   - non-blocking connect + poll() with a 10s hard cap so a dead LLM
 *     endpoint cannot freeze the UI thread for the system-default ~75s;
 *   - snprintf return guard: if the body+headers > 8192 the send loop
 *     would otherwise read PAST the stack buffer and leak process memory
 *     to the server (CWE-125 stack over-read);
 *   - Content-Length / Transfer-Encoding: chunked parsing so HTTP/1.1
 *     streaming responses (llama-server default) do not leave chunk
 *     markers in the body and break JSON extraction. */
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

    /* Resolve (try direct IP first, then DNS). */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        he = gethostbyname(host);
        if (!he) { close(sock); return -1; }
        if (he->h_length != (int)sizeof(addr.sin_addr)) {
            close(sock); return -1; /* defensive: AF_INET must be 4 bytes */
        }
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
    }

    /* Non-blocking connect + poll(10s). The 10s cap is independent of
     * timeout_sec (which governs the recv/send phase) so a blackholed
     * endpoint cannot hold the UI thread for the system default ~75s. */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sock); return -1;
    }

    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(sock); return -1;
    }
    if (ret != 0) {
        struct pollfd pfd;
        int connect_timeout_ms = 10000; /* 10s hard cap */
        pfd.fd = sock;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, connect_timeout_ms);
        if (pr <= 0) { close(sock); return -1; } /* timeout or signal */
        int soerr = 0;
        socklen_t sl = sizeof(soerr);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0 ||
            soerr != 0) {
            close(sock); return -1;
        }
    }

    /* Back to blocking mode for the send/recv loops. */
    if (fcntl(sock, F_SETFL, flags) < 0) { close(sock); return -1; }

    int rcv_to = (timeout_sec > 0) ? timeout_sec : 30;
    struct timeval tv = { .tv_sec = rcv_to, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* Build HTTP request */
    int body_len = body ? (int)strlen(body) : 0;
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

    /* Guard: snprintf returns the would-be length. If body+headers >=
     * sizeof(request) the body was truncated and Content-Length would
     * lie; the send loop below would also read past request[] into the
     * stack, leaking caller frames (return addresses, possibly crypto
     * pointers / conversation pointers) to the server. Refuse. */
    if (req_len < 0 || (size_t)req_len >= sizeof(request)) {
        close(sock);
        return -1;
    }

    /* Send request */
    int sent = 0;
    while (sent < req_len) {
        ret = send(sock, request + sent, req_len - sent, 0);
        if (ret <= 0) { close(sock); return -1; }
        sent += ret;
    }

    /* Receive response: read until all headers (\r\n\r\n) are in the
     * buffer, then parse Content-Length / Transfer-Encoding and finish
     * the body accordingly. */
    int total = 0;
    int header_end = -1;   /* index of first body byte in response_buf */
    int content_length = -1;
    int chunked = 0;

    while (total < response_size - 1 && header_end < 0) {
        ret = recv(sock, response_buf + total, response_size - 1 - total, 0);
        if (ret <= 0) { close(sock); return -1; }
        total += ret;
        response_buf[total] = '\0';
        char *he_ptr = strstr(response_buf, "\r\n\r\n");
        if (he_ptr) {
            header_end = (int)(he_ptr - response_buf) + 4;
        }
    }
    if (header_end < 0) { close(sock); return -1; } /* no headers */

    /* Parse headers (case-insensitive). Temporarily null-terminate the
     * header region so strstr/strcasestr do not run into the body. */
    {
        char *hdr_term = response_buf + header_end - 4; /* \r\n\r\n */
        char saved = *hdr_term;
        *hdr_term = '\0';
        char *cl = strcasestr(response_buf, "Content-Length:");
        if (cl) {
            cl += strlen("Content-Length:");
            content_length = atoi(cl);
        }
        char *te = strcasestr(response_buf, "Transfer-Encoding:");
        if (te) {
            char *v = te + strlen("Transfer-Encoding:");
            while (*v == ' ' || *v == '\t') v++;
            if (strncasecmp(v, "chunked", 7) == 0) chunked = 1;
        }
        *hdr_term = saved; /* restore */
    }

    if (chunked) {
        /* Keep recv'ing until we see the end-of-chunks marker (0\r\n\r\n)
         * or the socket closes, then dechunk in place. */
        for (;;) {
            response_buf[total] = '\0';
            if (strstr(response_buf + header_end, "0\r\n\r\n") != NULL)
                break;
            if (total >= response_size - 1) break;
            ret = recv(sock, response_buf + total,
                       response_size - 1 - total, 0);
            if (ret <= 0) break;
            total += ret;
        }
        int in_off = header_end;
        int out_off = header_end;
        while (in_off < total) {
            char *eol = memchr(response_buf + in_off, '\r',
                               (size_t)(total - in_off));
            if (!eol || eol + 1 >= response_buf + total || eol[1] != '\n')
                break;
            long chunk = strtol(response_buf + in_off, NULL, 16);
            char *data_start = eol + 2;
            if (chunk == 0) break;
            if (data_start + chunk > response_buf + total) break; /* incomplete */
            memmove(response_buf + out_off, data_start, (size_t)chunk);
            out_off += (int)chunk;
            in_off = (int)(data_start - response_buf) + (int)chunk + 2;
        }
        total = out_off;
    } else if (content_length >= 0) {
        /* Read exactly Content-Length body bytes. */
        int body_have = total - header_end;
        int body_need = content_length - body_have;
        while (body_need > 0 && total < response_size - 1) {
            int to_read = body_need;
            if (to_read > response_size - 1 - total)
                to_read = response_size - 1 - total;
            ret = recv(sock, response_buf + total, to_read, 0);
            if (ret <= 0) break;
            total += ret;
            body_need -= ret;
        }
    } else {
        /* No Content-Length, not chunked: read until close. */
        while (total < response_size - 1) {
            ret = recv(sock, response_buf + total,
                       response_size - 1 - total, 0);
            if (ret <= 0) break;
            total += ret;
        }
    }
    response_buf[total] = '\0';
    close(sock);

    /* Parse HTTP status code from first line */
    int status_code = -1;
    if (total > 12 && strncmp(response_buf, "HTTP/", 5) == 0) {
        status_code = atoi(response_buf + 9); /* "HTTP/1.1 XXX" -> skip to XXX */
    }

    /* Move body past headers (find \r\n\r\n) */
    char *body_start = strstr(response_buf, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_len_resp = total - (int)(body_start - response_buf);
        if (body_len_resp < 0) body_len_resp = 0;
        memmove(response_buf, body_start, (size_t)body_len_resp);
        response_buf[body_len_resp] = '\0';
    }

    return status_code;
}

/* HTTP GET (for simple health checks) */
int wlt_http_get(const char *url, char *response_buf, int response_size, int timeout_sec) {
    return wlt_http_post(url, NULL, NULL, response_buf, response_size, timeout_sec);
}
