/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wlt-tor-mgr: Tor Control Port Protocol
 *
 * Implements the Tor control protocol (control-spec.txt) over the abstract
 * transport. In WLTIOS production the transport is seL4 IPC to the Tor VM;
 * the default TCP transport is supplied for protocol-level testing against a
 * real `tor --ControlPort`.
 *
 * Covers: PROTOCOLINFO, AUTHENTICATE (cookie / null), GETINFO, SETCONF,
 * RESETCONF, SIGNAL and a generic line-command reader that correctly
 * reassembles multi-line ("+") replies.
 */
#include "tor_mgr.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ---- TCP transport (dev/test only) ------------------------------------- */
typedef struct {
    int fd;
} tcp_ctx_t;

static int tcp_connect(wlt_tor_transport_t *self, const char *host, uint16_t port)
{
    tcp_ctx_t *c = self ? self->ctx : NULL;
    struct addrinfo hints, *res = NULL, *rp;
    char portbuf[16];
    int rc, fd = -1;

    if (!c)
        return -EINVAL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%u", (unsigned)port);

    rc = getaddrinfo(host, portbuf, &hints, &res);
    if (rc != 0) {
        wlt_log_err("getaddrinfo(%s:%u): %s", host, (unsigned)port,
                    gai_strerror(rc));
        return -EHOSTUNREACH;
    }
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) {
        wlt_log_err("tcp_connect: no route to %s:%u", host, (unsigned)port);
        return -ECONNREFUSED;
    }
    /* Apply a sane receive timeout so poll loops cannot block forever. */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    c->fd = fd;
    return 0;
}

static ssize_t tcp_send(wlt_tor_transport_t *self, const void *buf, size_t len)
{
    tcp_ctx_t *c = self ? self->ctx : NULL;
    if (!c || c->fd < 0)
        return -ENOTCONN;
    return send(c->fd, buf, len, 0);
}

static ssize_t tcp_recv(wlt_tor_transport_t *self, void *buf, size_t len)
{
    tcp_ctx_t *c = self ? self->ctx : NULL;
    if (!c || c->fd < 0)
        return -ENOTCONN;
    return recv(c->fd, buf, len, 0);
}

static int tcp_close(wlt_tor_transport_t *self)
{
    tcp_ctx_t *c = self ? self->ctx : NULL;
    if (c && c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
    return 0;
}

static wlt_tor_transport_t *g_tcp; /* singleton */

wlt_tor_transport_t *wlt_tor_transport_tcp(void)
{
    if (g_tcp)
        return g_tcp;
    wlt_tor_transport_t *t = calloc(1, sizeof(*t));
    tcp_ctx_t *c = calloc(1, sizeof(*c));
    if (!t || !c) {
        free(t);
        free(c);
        return NULL;
    }
    c->fd = -1;
    t->connect = tcp_connect;
    t->send    = tcp_send;
    t->recv    = tcp_recv;
    t->close   = tcp_close;
    t->ctx     = c;
    g_tcp = t;
    return t;
}

/* ---- Helpers ------------------------------------------------------------ */
static ssize_t read_line(wlt_tor_transport_t *tp, char *buf, size_t len)
{
    size_t off = 0;
    while (off + 1 < len) {
        char ch;
        ssize_t n = tp->recv(tp, &ch, 1);
        if (n <= 0)
            return n < 0 ? n : (ssize_t)off; /* EOF: return what we have */
        if (ch == '\n')
            break;
        if (ch != '\r')
            buf[off++] = ch;
    }
    buf[off] = '\0';
    return (ssize_t)off;
}

static int send_all(wlt_tor_transport_t *tp, const char *data, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = tp->send(tp, data + off, len - off);
        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            return -EIO;
        }
        off += (size_t)n;
    }
    return 0;
}

/* Hex-encode binary cookie into upper-case hex string. */
static void hex_encode(const unsigned char *in, size_t n, char *out)
{
    static const char *h = "0123456789ABCDEF";
    for (size_t i = 0; i < n; i++) {
        out[2 * i]     = h[in[i] >> 4];
        out[2 * i + 1] = h[in[i] & 0x0f];
    }
    out[2 * n] = '\0';
}

/* ---- Reply reader ------------------------------------------------------- *
 * Reads reply lines until a "final" line ("NNN ..." with a space after the
 * code). Continuation lines ("NNN-...") and multi-line data blocks
 * ("NNN+..." terminated by a line containing only ".") are flattened into the
 * reply buffer separated by '\n'. Returns the 3-digit status code.
 */
static int read_reply(wlt_tor_control_t *ctl, char *reply, size_t reply_len)
{
    char line[WLT_TOR_MAX_LINE];
    size_t out_len = 0;
    int code = 0;
    bool in_data = false;

    reply[0] = '\0';
    for (;;) {
        ssize_t n = read_line(ctl->tp, line, sizeof(line));
        if (n < 0)
            return -EIO;
        if (n == 0 && code == 0)
            return -ECONNRESET;

        if (in_data) {
            /* Inside a "+..." block: a line that is exactly "." ends it. */
            if (strcmp(line, ".") == 0) {
                in_data = false;
                continue;
            }
            /* De-escape leading "." per control-spec. */
            const char *payload = line;
            if (payload[0] == '.')
                payload++;
            if (out_len + strlen(payload) + 1 < reply_len) {
                if (out_len)
                    reply[out_len++] = '\n';
                size_t l = strlen(payload);
                memcpy(reply + out_len, payload, l);
                out_len += l;
                reply[out_len] = '\0';
            }
            continue;
        }

        if (strlen(line) < 4)
            continue;
        char cstr[4] = { line[0], line[1], line[2], '\0' };
        char sep     = line[3];
        int  c       = atoi(cstr);
        if (c)
            code = c;

        const char *rest = line + 4;
        if (sep == '+') {
            in_data = true;
            if (out_len + strlen(rest) + 1 < reply_len) {
                if (out_len)
                    reply[out_len++] = '\n';
                size_t l = strlen(rest);
                memcpy(reply + out_len, rest, l);
                out_len += l;
                reply[out_len] = '\0';
            }
            continue;
        }
        if (sep == '-') {
            if (out_len + strlen(rest) + 1 < reply_len) {
                if (out_len)
                    reply[out_len++] = '\n';
                size_t l = strlen(rest);
                memcpy(reply + out_len, rest, l);
                out_len += l;
                reply[out_len] = '\0';
            }
            continue;
        }
        if (sep == ' ') {
            if (out_len + strlen(rest) + 1 < reply_len) {
                if (out_len)
                    reply[out_len++] = '\n';
                size_t l = strlen(rest);
                memcpy(reply + out_len, rest, l);
                out_len += l;
                reply[out_len] = '\0';
            }
            break; /* final line */
        }
    }
    return code;
}

/* ---- Public control API ------------------------------------------------- */
int wlt_tor_control_open(wlt_tor_control_t *ctl, wlt_tor_transport_t *tp,
                         const char *host, uint16_t port)
{
    char reply[WLT_TOR_MAX_REPLY];
    int rc;

    if (!ctl || !tp || !host)
        return -EINVAL;
    memset(ctl, 0, sizeof(*ctl));
    ctl->tp = tp;

    rc = tp->connect(tp, host, port);
    if (rc < 0)
        return rc;

    /* PROTOCOLINFO tells us the supported auth methods + cookie location. */
    rc = wlt_tor_control_command(ctl, "PROTOCOLINFO 1", reply, sizeof(reply));
    if (rc != 250) {
        wlt_log_err("PROTOCOLINFO failed: code=%d", rc);
        return -EPROTO;
    }
    /* Best-effort capture of the server software line. */
    char *p = strstr(reply, "VERSION Tor=");
    if (p) {
        p += strlen("VERSION Tor=");
        size_t i = 0;
        while (p[i] && p[i] != ' ' && p[i] != '\n' &&
               i + 1 < sizeof(ctl->server_version)) {
            ctl->server_version[i] = p[i];
            i++;
        }
        ctl->server_version[i] = '\0';
    }
    ctl->owns_transport = false;
    wlt_log_inf("control port open (%s:%u) tor=%s", host, (unsigned)port,
                ctl->server_version[0] ? ctl->server_version : "?");
    return 0;
}

int wlt_tor_control_authenticate_cookie(wlt_tor_control_t *ctl,
                                        const char *cookie_path)
{
    char reply[WLT_TOR_MAX_REPLY];
    char cmd[16 + 2 * 64 + 4]; /* "AUTHENTICATE " + hex(64) + NUL + slack */
    unsigned char cookie[64];
    char hex[2 * sizeof(cookie) + 1];
    int rc, fd;
    ssize_t n;

    if (!ctl || !cookie_path)
        return -EINVAL;

    fd = open(cookie_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        wlt_log_wrn("cookie file %s: %s", cookie_path, strerror(errno));
        return -errno;
    }
    n = read(fd, cookie, sizeof(cookie));
    close(fd);
    if (n <= 0) {
        wlt_log_err("cookie file %s empty/unreadable", cookie_path);
        return -EIO;
    }
    hex_encode(cookie, (size_t)n, hex);
    snprintf(cmd, sizeof(cmd), "AUTHENTICATE %s", hex);

    rc = wlt_tor_control_command(ctl, cmd, reply, sizeof(reply));
    if (rc != 250) {
        wlt_log_err("AUTHENTICATE (cookie) rejected: %d %s", rc, reply);
        return -EACCES;
    }
    ctl->authenticated = true;
    wlt_log_dbg("authenticated via cookie (%zu bytes)", (size_t)n);
    return 0;
}

int wlt_tor_control_authenticate_null(wlt_tor_control_t *ctl)
{
    char reply[WLT_TOR_MAX_REPLY];
    int rc;
    if (!ctl)
        return -EINVAL;
    rc = wlt_tor_control_command(ctl, "AUTHENTICATE", reply, sizeof(reply));
    if (rc != 250) {
        wlt_log_err("AUTHENTICATE (null) rejected: %d %s", rc, reply);
        return -EACCES;
    }
    ctl->authenticated = true;
    return 0;
}

int wlt_tor_control_command(wlt_tor_control_t *ctl, const char *cmd,
                            char *reply, size_t reply_len)
{
    char buf[WLT_TOR_MAX_LINE + 2];
    int rc;

    if (!ctl || !ctl->tp || !cmd)
        return -EINVAL;
    if (reply && reply_len)
        reply[0] = '\0';

    rc = snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    if (rc < 0 || (size_t)rc >= sizeof(buf))
        return -ENAMETOOLONG;
    rc = send_all(ctl->tp, buf, strlen(buf));
    if (rc < 0)
        return rc;

    if (!reply || reply_len == 0)
        return 250; /* caller does not want the body */
    return read_reply(ctl, reply, reply_len);
}

int wlt_tor_control_getinfo(wlt_tor_control_t *ctl, const char *key,
                            char *out, size_t out_len)
{
    char cmd[WLT_TOR_MAX_LINE];
    char reply[WLT_TOR_MAX_REPLY];
    int rc;

    if (!ctl || !key)
        return -EINVAL;
    snprintf(cmd, sizeof(cmd), "GETINFO %s", key);
    rc = wlt_tor_control_command(ctl, cmd, reply, sizeof(reply));
    if (rc != 250)
        return rc < 0 ? rc : -EPROTO;

    /* Reply payload looks like "key=value\nOK". Extract the value. */
    char needle[128];
    snprintf(needle, sizeof(needle), "%s=", key);
    char *p = strstr(reply, needle);
    if (!p) {
        if (out && out_len)
            out[0] = '\0';
        return -ENOENT;
    }
    p += strlen(needle);
    size_t i = 0;
    while (p[i] && p[i] != '\n' && i + 1 < out_len) {
        out[i] = p[i];
        i++;
    }
    if (out_len)
        out[i] = '\0';
    return 0;
}

int wlt_tor_control_setconf(wlt_tor_control_t *ctl, const char *key,
                            const char *value)
{
    char cmd[WLT_TOR_MAX_LINE];
    char reply[WLT_TOR_MAX_REPLY];

    if (!ctl || !key)
        return -EINVAL;
    if (value && value[0])
        snprintf(cmd, sizeof(cmd), "SETCONF %s=%s", key, value);
    else
        snprintf(cmd, sizeof(cmd), "SETCONF %s", key);
    int rc = wlt_tor_control_command(ctl, cmd, reply, sizeof(reply));
    if (rc != 250) {
        wlt_log_wrn("SETCONF %s failed: %d %s", key, rc, reply);
        return -EPROTO;
    }
    return 0;
}

int wlt_tor_control_resetconf(wlt_tor_control_t *ctl, const char *key)
{
    char cmd[128];
    char reply[WLT_TOR_MAX_REPLY];
    if (!ctl || !key)
        return -EINVAL;
    snprintf(cmd, sizeof(cmd), "RESETCONF %s", key);
    int rc = wlt_tor_control_command(ctl, cmd, reply, sizeof(reply));
    return rc == 250 ? 0 : -EPROTO;
}

int wlt_tor_control_signal(wlt_tor_control_t *ctl, const char *sig)
{
    char cmd[64];
    char reply[WLT_TOR_MAX_REPLY];
    if (!ctl || !sig)
        return -EINVAL;
    snprintf(cmd, sizeof(cmd), "SIGNAL %s", sig);
    int rc = wlt_tor_control_command(ctl, cmd, reply, sizeof(reply));
    if (rc != 250) {
        wlt_log_wrn("SIGNAL %s failed: %d %s", sig, rc, reply);
        return -EPROTO;
    }
    return 0;
}

int wlt_tor_control_close(wlt_tor_control_t *ctl)
{
    if (!ctl)
        return -EINVAL;
    if (ctl->tp) {
        /* Be polite; ignore failures. */
        wlt_tor_control_command(ctl, "QUIT", NULL, 0);
        ctl->tp->close(ctl->tp);
        ctl->tp = NULL;
    }
    ctl->authenticated = false;
    return 0;
}
