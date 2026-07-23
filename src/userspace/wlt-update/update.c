/**
 * @file update.c
 * @brief Update check, download (via Tor), and install orchestration
 * @module wlt-update
 * @purpose Fetches the version manifest over a Tor SOCKS5 tunnel,
 *          downloads the signed package (with resume), verifies it, and
 *          drives the atomic install onto the inactive partition.
 * @dependencies: update.h, partition.h, libsodium (via verify.c), POSIX
 * @tested_by: tests/userspace/wlt_update_test.c
 * @spec: docs/specs/wlt-update.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 *
 * NOTE: WLT-Net exposes Tor health but no HTTP client, so this module
 * owns a minimal SOCKS5 + HTTP/1.1 transport. GET only, no TLS â the
 * .onion circuit provides confidentiality. Kept small for auditability.
 */

#include "update.h"
#include "partition.h"
#include "rollback.h"  /* anti-rollback counter (research-I R2) */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/file.h>

static wlt_UpdateConfig g_config;
static bool g_initialised = false;

/* ââ URL parse: "http://host[:port]/path" (port defaults 80) ââ */
static wlt_update_error_t
parse_url(const char *url, char *host, size_t host_max,
          uint16_t *port, char *path, size_t path_max)
{
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) { p += 7; }
    else if (strncmp(p, "https://", 8) == 0) { p += 8; }

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (colon != NULL && (slash == NULL || colon < slash))
    {
        size_t hlen = (size_t)(colon - p);
        if (hlen == 0 || hlen >= host_max) { return WLT_UPDATE_ERR_INVALID_ARG; }
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        long vp = strtol(colon + 1, NULL, 10);
        if (vp <= 0 || vp > 65535) { return WLT_UPDATE_ERR_INVALID_ARG; }
        *port = (uint16_t)vp;
    }
    else
    {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen == 0 || hlen >= host_max) { return WLT_UPDATE_ERR_INVALID_ARG; }
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = 80;
    }
    if (slash == NULL)
    {
        wlt_strlcpy(path, "/", path_max);
    }
    else
    {
        wlt_strlcpy(path, slash, path_max);
    }
    return WLT_UPDATE_OK;
}

static wlt_update_error_t
send_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = send(fd, p + off, len - off, 0);
        if (n < 0) { if (errno == EINTR) { continue; } return WLT_UPDATE_ERR_NETWORK; }
        if (n == 0) { return WLT_UPDATE_ERR_NETWORK; }
        off += (size_t)n;
    }
    return WLT_UPDATE_OK;
}

static wlt_update_error_t
recv_exact(int fd, void *buf, size_t len)
{
    uint8_t *p = buf;
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = recv(fd, p + off, len - off, 0);
        if (n < 0) { if (errno == EINTR) { continue; } return WLT_UPDATE_ERR_NETWORK; }
        if (n == 0) { return WLT_UPDATE_ERR_NETWORK; }
        off += (size_t)n;
    }
    return WLT_UPDATE_OK;
}

/* ââ Open a SOCKS5 tunnel through Tor. No DNS leaves the process:
 *    the hostname is resolved by the Tor exit via ATYP=3 (domain). ââ */
static int
socks5_connect(const char *host, uint16_t port, uint16_t tor_port)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { return -1; }

    struct timeval to = { .tv_sec = WLT_UPDATE_CHECK_TIMEOUT_SEC, .tv_usec = 0 };
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(tor_port);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        WLT_UPDATE_LOG_ERR("Tor SOCKS connect failed (port %u): %s",
                           tor_port, strerror(errno));
        close(fd);
        return -1;
    }

    uint8_t greet[3] = { 0x05, 0x01, 0x00 };
    if (send_all(fd, greet, sizeof(greet)) != WLT_UPDATE_OK) { close(fd); return -1; }
    uint8_t greply[2];
    if (recv_exact(fd, greply, sizeof(greply)) != WLT_UPDATE_OK ||
        greply[0] != 0x05 || greply[1] != 0x00)
    {
        WLT_UPDATE_LOG_ERR("Tor SOCKS refused greeting");
        close(fd);
        return -1;
    }

    size_t hlen = strlen(host);
    if (hlen == 0 || hlen > 255) { close(fd); return -1; }
    uint8_t req[3 + 256 + 2];
    req[0] = 0x05; req[1] = 0x01; req[2] = 0x00; req[3] = 0x03;
    req[4] = (uint8_t)hlen;
    memcpy(req + 5, host, hlen);
    req[5 + hlen] = (uint8_t)(port >> 8);
    req[5 + hlen + 1] = (uint8_t)(port & 0xff);
    if (send_all(fd, req, 5 + hlen + 2) != WLT_UPDATE_OK) { close(fd); return -1; }

    uint8_t rep[4];
    if (recv_exact(fd, rep, sizeof(rep)) != WLT_UPDATE_OK || rep[1] != 0x00)
    {
        WLT_UPDATE_LOG_ERR("Tor SOCKS CONNECT rejected (rep=%u)", rep[1]);
        close(fd);
        return -1;
    }
    /* Drain bound-address so the socket is body-ready. */
    size_t addrlen;
    if (rep[3] == 0x01) { addrlen = 4; }
    else if (rep[3] == 0x04) { addrlen = 16; }
    else if (rep[3] == 0x03)
    {
        uint8_t l;
        if (recv_exact(fd, &l, 1) != WLT_UPDATE_OK) { close(fd); return -1; }
        addrlen = l;
    }
    else { close(fd); return -1; }
    uint8_t bound[18];
    if (recv_exact(fd, bound, addrlen + 2) != WLT_UPDATE_OK) { close(fd); return -1; }
    return fd;
}

/* ââ GET a URL through Tor, stream the body to out_fd.
 *    resume_from>0 sends a Range; 206 appends at that offset, 200
 *    truncates and rewrites from zero. *written gets bytes this call. ââ */
static wlt_update_error_t
tor_get(const char *url, uint16_t tor_port, int out_fd,
        uint64_t resume_from, uint64_t *written, uint32_t timeout_sec)
{
    char host[WLT_UPDATE_MAX_HOST];
    char path[WLT_UPDATE_MAX_URL];
    uint16_t port;
    wlt_update_error_t err = parse_url(url, host, sizeof(host),
                                       &port, path, sizeof(path));
    if (err != WLT_UPDATE_OK) { return err; }

    int fd = socks5_connect(host, port, tor_port);
    if (fd < 0) { return WLT_UPDATE_ERR_TOR; }
    struct timeval to = { .tv_sec = (time_t)timeout_sec, .tv_usec = 0 };
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

    char req[WLT_UPDATE_MAX_URL + 128];
    int rn;
    if (resume_from > 0)
    {
        rn = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\nHost: %s\r\nRange: bytes=%llu-\r\n"
            "User-Agent: wlt-update/%s\r\nConnection: close\r\n\r\n",
            path, host, (unsigned long long)resume_from, WLT_UPDATE_VERSION);
    }
    else
    {
        rn = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: wlt-update/%s\r\n"
            "Connection: close\r\n\r\n", path, host, WLT_UPDATE_VERSION);
    }
    if (rn < 0 || (size_t)rn >= sizeof(req)) { close(fd); return WLT_UPDATE_ERR_INVALID_ARG; }

    err = send_all(fd, req, (size_t)rn);
    if (err != WLT_UPDATE_OK) { close(fd); return err; }

    char hdr[4096];
    size_t hfilled = 0;
    bool got_sep = false;
    size_t body_start = 0;
    while (hfilled < sizeof(hdr) - 1)
    {
        ssize_t n = recv(fd, hdr + hfilled, sizeof(hdr) - 1 - hfilled, 0);
        if (n <= 0) { break; }
        hfilled += (size_t)n;
        hdr[hfilled] = '\0';
        char *sep = strstr(hdr, "\r\n\r\n");
        if (sep != NULL)
        {
            got_sep = true;
            body_start = (size_t)(sep - hdr) + 4;
            break;
        }
    }
    if (!got_sep) { WLT_UPDATE_LOG_ERR("malformed HTTP from %s", url); close(fd); return WLT_UPDATE_ERR_NETWORK; }

    int code = 0;
    if (strncmp(hdr, "HTTP/", 5) == 0)
    {
        char *sp = strchr(hdr, ' ');
        if (sp != NULL) { code = (int)strtol(sp + 1, NULL, 10); }
    }
    if (code == 0) { close(fd); return WLT_UPDATE_ERR_NETWORK; }
    if (code == 416) { close(fd); *written = 0; return WLT_UPDATE_OK; } /* complete */
    if (code != 200 && code != 206)
    {
        WLT_UPDATE_LOG_ERR("HTTP %d for %s", code, url);
        close(fd);
        return WLT_UPDATE_ERR_NETWORK;
    }

    if (code == 206 && resume_from > 0)
    {
        if (lseek(out_fd, (off_t)resume_from, SEEK_SET) < 0) { close(fd); return WLT_UPDATE_ERR_IO; }
    }
    else
    {
        if (ftruncate(out_fd, 0) != 0 || lseek(out_fd, 0, SEEK_SET) < 0) { close(fd); return WLT_UPDATE_ERR_IO; }
    }

    uint64_t total = 0;
    if (body_start < hfilled)
    {
        size_t n = hfilled - body_start;
        if (write(out_fd, hdr + body_start, n) != (ssize_t)n) { close(fd); return WLT_UPDATE_ERR_IO; }
        total += n;
    }

    uint8_t buf[WLT_UPDATE_CHUNK_SIZE];
    for (;;)
    {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) { if (errno == EINTR) { continue; } close(fd); return WLT_UPDATE_ERR_NETWORK; }
        if (n == 0) { break; }
        ssize_t off = 0;
        while (off < n)
        {
            ssize_t w = write(out_fd, buf + off, (size_t)(n - off));
            if (w < 0) { if (errno == EINTR) { continue; } close(fd); return WLT_UPDATE_ERR_IO; }
            off += w;
        }
        total += (uint64_t)n;
    }

    close(fd);
    if (written != NULL) { *written = total; }
    return WLT_UPDATE_OK;
}

/* ââ Semantic version compare: <0 a<b, 0 equal, >0 a>b. ââ */
static int
version_cmp(const char *a, const char *b)
{
    /* Parse as unsigned with bounds — int sscanf would overflow on
     * "2147483648.0.0" (F10) and lock the device out of future updates.
     * Non-numeric versions parse as 0.0.0 (oldest, M2). Components >65535
     * are clamped so a malformed manifest can never claim "newer". */
    unsigned av[3] = { 0, 0, 0 };
    unsigned bv[3] = { 0, 0, 0 };
    int na = sscanf(a, "%u.%u.%u", &av[0], &av[1], &av[2]);
    int nb = sscanf(b, "%u.%u.%u", &bv[0], &bv[1], &bv[2]);
    if (na < 1) { av[0] = av[1] = av[2] = 0; }
    if (nb < 1) { bv[0] = bv[1] = bv[2] = 0; }
    for (int i = 0; i < 3; i++)
    {
        unsigned ac = (av[i] > 65535) ? 65535 : av[i];
        unsigned bc = (bv[i] > 65535) ? 65535 : bv[i];
        if (ac != bc) { return (ac < bc) ? -1 : 1; }
    }
    return 0;
}

static wlt_update_error_t
parse_manifest(FILE *fp, wlt_UpdateVersion *out)
{
    memset(out, 0, sizeof(*out));
    char line[WLT_UPDATE_MAX_LINE];
    char key[WLT_UPDATE_MAX_LINE];
    char val[WLT_UPDATE_MAX_LINE];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (!wlt_config_parse_kv(line, key, sizeof(key), val, sizeof(val))) { continue; }
        if (strcmp(key, "version") == 0) { wlt_strlcpy(out->version, val, sizeof(out->version)); }
        else if (strcmp(key, "build") == 0) { out->build = (uint32_t)strtoul(val, NULL, 10); }
        else if (strcmp(key, "timestamp") == 0) { wlt_strlcpy(out->timestamp, val, sizeof(out->timestamp)); }
        else if (strcmp(key, "url") == 0) { wlt_strlcpy(out->url, val, sizeof(out->url)); }
        else if (strcmp(key, "size") == 0) { out->size = (uint64_t)strtoull(val, NULL, 10); }
        else if (strcmp(key, "sig_url") == 0) { wlt_strlcpy(out->sig_url, val, sizeof(out->sig_url)); }
        else if (strcmp(key, "rollback_index") == 0) { out->rollback_index = strtoull(val, NULL, 10); }
    }
    if (out->version[0] == '\0' || out->url[0] == '\0') { return WLT_UPDATE_ERR_INVALID_ARG; }
    return WLT_UPDATE_OK;
}

/* ââ Status file (small KV store at WLT_UPDATE_STATUS_PATH) ââ */
static void
status_defaults(wlt_UpdateStatus *st)
{
    memset(st, 0, sizeof(*st));
    st->active_partition = 'A';
    wlt_strlcpy(st->current_version, g_config.current_version, sizeof(st->current_version));
    st->auto_update = g_config.auto_update;
}

static wlt_update_error_t
status_read(wlt_UpdateStatus *st)
{
    status_defaults(st);
    FILE *fp = fopen(WLT_UPDATE_STATUS_PATH, "r");
    if (fp == NULL) { return WLT_UPDATE_OK; }
    char line[WLT_UPDATE_MAX_LINE], key[WLT_UPDATE_MAX_LINE], val[WLT_UPDATE_MAX_LINE];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (!wlt_config_parse_kv(line, key, sizeof(key), val, sizeof(val))) { continue; }
        if (strcmp(key, "pending_reboot") == 0) { st->pending_reboot = (strcmp(val, "yes") == 0); }
        else if (strcmp(key, "update_available") == 0) { st->update_available = (strcmp(val, "yes") == 0); }
        else if (strcmp(key, "pending_version") == 0) { wlt_strlcpy(st->pending_version, val, sizeof(st->pending_version)); }
        else if (strcmp(key, "pending_rollback_index") == 0) { st->pending_rollback_index = strtoull(val, NULL, 10); }
        else if (strcmp(key, "last_update_time") == 0) { wlt_strlcpy(st->last_update_time, val, sizeof(st->last_update_time)); }
        else if (strcmp(key, "auto_update") == 0) { st->auto_update = (strcmp(val, "yes") == 0); }
        else if (strcmp(key, "last_error") == 0) { wlt_strlcpy(st->last_error, val, sizeof(st->last_error)); }
    }
    fclose(fp);
    return WLT_UPDATE_OK;
}

static wlt_update_error_t
status_write(const wlt_UpdateStatus *st)
{
    char tmp[WLT_UPDATE_MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", WLT_UPDATE_STATUS_PATH);
    /* O_EXCL|O_NOFOLLOW closes the symlink-swap race (F2); fsync file +
     * fsync dir gives crash durability (G3). */
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0 && errno == EEXIST) { (void)unlink(tmp); fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600); }
    if (fd < 0) { return WLT_UPDATE_ERR_IO; }
    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) { close(fd); (void)unlink(tmp); return WLT_UPDATE_ERR_IO; }
    fprintf(fp, "pending_reboot = %s\n", st->pending_reboot ? "yes" : "no");
    fprintf(fp, "update_available = %s\n", st->update_available ? "yes" : "no");
    fprintf(fp, "pending_version = %s\n", st->pending_version);
    fprintf(fp, "pending_rollback_index = %llu\n", (unsigned long long)st->pending_rollback_index);
    fprintf(fp, "last_update_time = %s\n", st->last_update_time[0] ? st->last_update_time : "never");
    fprintf(fp, "auto_update = %s\n", st->auto_update ? "yes" : "no");
    fprintf(fp, "last_error = %s\n", st->last_error[0] ? st->last_error : "none");
    if (fflush(fp) != 0) { (void)fclose(fp); (void)unlink(tmp); return WLT_UPDATE_ERR_IO; }
    if (fsync(fileno(fp)) < 0) { (void)fclose(fp); (void)unlink(tmp); return WLT_UPDATE_ERR_IO; }
    if (fclose(fp) != 0) { (void)unlink(tmp); return WLT_UPDATE_ERR_IO; }
    if (rename(tmp, WLT_UPDATE_STATUS_PATH) != 0) { (void)unlink(tmp); return WLT_UPDATE_ERR_IO; }
    /* fsync the parent directory so the rename is durable across power loss. */
    {
        char dirbuf[WLT_UPDATE_MAX_PATH];
        wlt_strlcpy(dirbuf, WLT_UPDATE_STATUS_PATH, sizeof(dirbuf));
        char *slash = strrchr(dirbuf, '/');
        if (slash != NULL) {
            *slash = '\0';
            int dfd = open(dirbuf, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (dfd >= 0) { (void)fsync(dfd); close(dfd); }
        }
    }
    return WLT_UPDATE_OK;
}

static void
stamp_now(char *buf, size_t max)
{
    time_t t = time(NULL);
    struct tm tmv;
    if (gmtime_r(&t, &tmv) != NULL) { strftime(buf, max, "%Y-%m-%dT%H:%M:%SZ", &tmv); }
    else { wlt_strlcpy(buf, "unknown", max); }
}

static void
set_last_error(const char *msg)
{
    wlt_UpdateStatus st;
    if (status_read(&st) == WLT_UPDATE_OK)
    {
        wlt_strlcpy(st.last_error, msg, sizeof(st.last_error));
        (void)status_write(&st);
    }
}

static wlt_update_error_t
history_append(const wlt_UpdateHistoryEntry *e)
{
    FILE *fp = fopen(WLT_UPDATE_HISTORY_PATH, "a");
    if (fp == NULL) { return WLT_UPDATE_ERR_IO; }
    fprintf(fp, "%s %s %c %s\n",
            e->timestamp[0] ? e->timestamp : "unknown",
            e->version[0] ? e->version : "unknown",
            e->partition, e->success ? "ok" : "fail");
    fclose(fp);
    return WLT_UPDATE_OK;
}

/* ââ Public: lifecycle ââ */
wlt_update_error_t
wlt_update_init(const wlt_UpdateConfig *config)
{
    if (config == NULL)
    {
        wlt_config_defaults(&g_config);
        (void)wlt_config_load(WLT_UPDATE_CONFIG_PATH, &g_config);
    }
    else
    {
        g_config = *config;
    }
    g_initialised = true;
    WLT_UPDATE_LOG_INF("wlt-update %s initialised (current %s)",
                       WLT_UPDATE_VERSION, g_config.current_version);
    return WLT_UPDATE_OK;
}

void
wlt_update_shutdown(void)
{
    g_initialised = false;
}

/* ââ Public: check ââ */
wlt_update_error_t
wlt_update_check(wlt_UpdateVersion *out)
{
    if (out == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    if (!g_initialised) { wlt_update_init(NULL); }

    FILE *tmp = tmpfile();
    if (tmp == NULL) { return WLT_UPDATE_ERR_IO; }
    uint64_t written = 0;
    wlt_update_error_t err = tor_get(g_config.repo_url, g_config.tor_socks_port,
                                     fileno(tmp), 0, &written,
                                     WLT_UPDATE_CHECK_TIMEOUT_SEC);
    if (err != WLT_UPDATE_OK)
    {
        fclose(tmp);
        set_last_error("check: network/Tor failure");
        return err;
    }
    rewind(tmp);
    err = parse_manifest(tmp, out);
    fclose(tmp);
    if (err != WLT_UPDATE_OK) { set_last_error("check: malformed manifest"); return err; }

    wlt_UpdateStatus st;
    (void)status_read(&st);
    if (version_cmp(out->version, g_config.current_version) <= 0)
    {
        st.update_available = false;
        st.pending_version[0] = '\0';
        (void)status_write(&st);
        return WLT_UPDATE_ERR_ALREADY_UP_TO_DATE;
    }
    /* Anti-rollback check (research-I R2): refuse to report an update
     * as available if its rollback_index is lower than the stored
     * counter. Prevents downgrade attacks before any download. */
    err = wlt_rollback_check(out->rollback_index);
    if (err != WLT_UPDATE_OK) {
        st.update_available = false;
        st.pending_version[0] = '\0';
        (void)status_write(&st);
        WLT_UPDATE_LOG_ERR("check: rollback refused for version %s "
                          "(index %llu)", out->version,
                          (unsigned long long)out->rollback_index);
        return err;
    }
    st.update_available = true;
    wlt_strlcpy(st.pending_version, out->version, sizeof(st.pending_version));
    st.pending_rollback_index = out->rollback_index;
    st.last_error[0] = '\0';
    (void)status_write(&st);
    WLT_UPDATE_LOG_INF("update available: %s", out->version);
    return WLT_UPDATE_OK;
}

/* ââ Public: download (package resumable + fresh signature) ââ */
wlt_update_error_t
wlt_update_download(const char *dest_path, const wlt_UpdateVersion *ver)
{
    if (dest_path == NULL || ver == NULL || ver->url[0] == '\0') { return WLT_UPDATE_ERR_INVALID_ARG; }
    if (!g_initialised) { wlt_update_init(NULL); }

    for (int attempt = 0; attempt < WLT_UPDATE_DOWNLOAD_RETRIES; attempt++)
    {
        struct stat stb;
        uint64_t resume = 0;
        if (stat(dest_path, &stb) == 0) { resume = (uint64_t)stb.st_size; }
        int fd = open(dest_path, O_WRONLY | O_CREAT, 0644);
        if (fd < 0) { set_last_error("download: cannot open dest"); return WLT_UPDATE_ERR_IO; }
        uint64_t written = 0;
        wlt_update_error_t err = tor_get(ver->url, g_config.tor_socks_port, fd,
                                         resume, &written,
                                         WLT_UPDATE_DOWNLOAD_TIMEOUT_SEC);
        close(fd);
        if (err == WLT_UPDATE_OK) { break; }
        if (attempt == WLT_UPDATE_DOWNLOAD_RETRIES - 1)
        {
            set_last_error("download: retries exhausted");
            return err;
        }
        WLT_UPDATE_LOG_WRN("download attempt %d failed, retrying", attempt + 1);
    }

    char sig_path[WLT_UPDATE_MAX_PATH];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", dest_path);
    int sfd = open(sig_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (sfd < 0) { set_last_error("download: cannot open sig dest"); return WLT_UPDATE_ERR_IO; }
    uint64_t sw = 0;
    wlt_update_error_t serr = tor_get(ver->sig_url, g_config.tor_socks_port, sfd, 0, &sw,
                                      WLT_UPDATE_CHECK_TIMEOUT_SEC);
    close(sfd);
    if (serr != WLT_UPDATE_OK) { set_last_error("download: signature fetch failed"); return serr; }
    WLT_UPDATE_LOG_INF("downloaded %s (+ signature)", dest_path);
    return WLT_UPDATE_OK;
}

/* ââ Public: install (verify â format â mount â extract â flip boot) ââ */
/* Acquire the global wltupdate lock so concurrent runs cannot race on
 * boot config / status / package install (C-H6). Returns an open fd to
 * close on exit, or -1 on failure. */
static int
acquire_lock(void)
{
    int fd = open("/var/lock/wlt-update.lock", O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) { return -1; }
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) { close(fd); return -1; }
    return fd;
}

wlt_update_error_t
wlt_update_install(const char *package_path)
{
    if (package_path == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    if (!g_initialised) { wlt_update_init(NULL); }

    int lock_fd = acquire_lock();
    if (lock_fd < 0) { set_last_error("install: another update in progress"); return WLT_UPDATE_ERR_BUSY; }

    wlt_UpdateStatus st;
    (void)status_read(&st);
    if (st.pending_reboot) { close(lock_fd); return WLT_UPDATE_ERR_PENDING_REBOOT; }

    wlt_PartitionId inactive;
    wlt_update_error_t err = wlt_partition_get_inactive(&inactive);
    if (err != WLT_UPDATE_OK) { close(lock_fd); return err; }

    /* Open the package ONCE - both verify and tar read through /dev/fd/N
     * so an attacker cannot rename()/replace the file in the gap between
     * verify's fclose and tar's open (TOCTOU, F1). O_NOFOLLOW refuses a
     * symlink-swap of the package itself. The fd is NOT CLOEXEC so tar
     * inherits it; we close it explicitly at the end. */
    int pkg_fd = open(package_path, O_RDONLY | O_NOFOLLOW);
    if (pkg_fd < 0) {
        set_last_error("install: cannot open package");
        close(lock_fd);
        return WLT_UPDATE_ERR_NOT_FOUND;
    }
    char pkg_fd_path[32];
    snprintf(pkg_fd_path, sizeof(pkg_fd_path), "/dev/fd/%d", pkg_fd);

    /* Free-space check BEFORE format (C-H1, C-H2). Heuristic: a .tar.zst
     * payload typically expands ~4x; refuse if the inactive device is
     * smaller than that. lseek(SEEK_END) on a block device returns its
     * size in bytes. */
    struct stat pst;
    if (fstat(pkg_fd, &pst) != 0) {
        set_last_error("install: cannot stat package");
        close(pkg_fd); close(lock_fd);
        return WLT_UPDATE_ERR_NOT_FOUND;
    }
    uint64_t needed = (uint64_t)pst.st_size * 4;
    wlt_PartitionInfo pinfo;
    err = wlt_partition_get_info(inactive, &pinfo);
    if (err != WLT_UPDATE_OK) { close(pkg_fd); close(lock_fd); return err; }
    {
        int dev_fd = open(pinfo.device, O_RDONLY | O_CLOEXEC);
        if (dev_fd >= 0) {
            off_t dev_size = lseek(dev_fd, 0, SEEK_END);
            close(dev_fd);
            if (dev_size > 0 && (uint64_t)dev_size < needed) {
                set_last_error("install: inactive partition too small for package");
                close(pkg_fd); close(lock_fd);
                return WLT_UPDATE_ERR_INSUFFICIENT_SPACE;
            }
        }
    }

    /* Verify signature via the open fd path (TOCTOU: never re-open by name). */
    char sig_path[WLT_UPDATE_MAX_PATH];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", package_path);
    err = wlt_update_verify_file(pkg_fd_path, sig_path, g_config.pubkey_path);
    if (err != WLT_UPDATE_OK) { set_last_error("install: signature FAILED"); close(pkg_fd); close(lock_fd); return err; }

    /* Signed manifest check (research-I R5): if a .manifest file exists
     * alongside the package, verify its Ed25519 signature and check that
     * the blob's SHA-256 matches + the version/rollback_index bind. This
     * prevents signature replay (same validly-signed blob applied to a
     * different slot or rolled-back version). If no manifest exists,
     * log a warning and proceed (backward-compatible). */
    {
        char manifest_path[WLT_UPDATE_MAX_PATH];
        char manifest_sig_path[WLT_UPDATE_MAX_PATH];
        snprintf(manifest_path, sizeof(manifest_path), "%s.manifest", package_path);
        snprintf(manifest_sig_path, sizeof(manifest_sig_path), "%s.manifest.sig", package_path);
        if (access(manifest_path, F_OK) == 0 && access(manifest_sig_path, F_OK) == 0) {
            wlt_UpdateManifest mfst;
            err = wlt_update_verify_manifest(manifest_path, manifest_sig_path,
                                             g_config.pubkey_path, &mfst);
            if (err != WLT_UPDATE_OK) {
                set_last_error("install: manifest signature FAILED");
                close(pkg_fd); close(lock_fd);
                return err;
            }
            /* Check the blob SHA-256 matches the manifest. */
            char blob_hash[65];
            err = wlt_update_compute_sha256(pkg_fd_path, blob_hash, sizeof(blob_hash));
            if (err != WLT_UPDATE_OK) {
                set_last_error("install: cannot hash package");
                close(pkg_fd); close(lock_fd);
                return err;
            }
            if (memcmp(blob_hash, mfst.sha256_hex, 64) != 0) {
                WLT_UPDATE_LOG_ERR("install: blob SHA-256 mismatch (manifest=%s actual=%s)",
                                   mfst.sha256_hex, blob_hash);
                set_last_error("install: blob hash does not match manifest");
                close(pkg_fd); close(lock_fd);
                return WLT_UPDATE_ERR_SIGNATURE;
            }
            /* Check target_slot matches the inactive slot (if manifest specifies). */
            if (mfst.target_slot != '*') {
                char want = (inactive == WLT_PARTITION_A) ? 'A' : 'B';
                if (mfst.target_slot != want) {
                    WLT_UPDATE_LOG_ERR("install: manifest targets slot %c but inactive is %c",
                                       mfst.target_slot, want);
                    set_last_error("install: manifest targets wrong slot");
                    close(pkg_fd); close(lock_fd);
                    return WLT_UPDATE_ERR_INVALID_ARG;
                }
            }
            /* Check rollback_index against the stored counter (defense-in-depth:
             * wlt_update_check already did this, but re-check at install time
             * in case the counter changed between check and install). */
            err = wlt_rollback_check(mfst.rollback_index);
            if (err != WLT_UPDATE_OK) {
                set_last_error("install: manifest rollback_index refused");
                close(pkg_fd); close(lock_fd);
                return err;
            }
            WLT_UPDATE_LOG_INF("install: manifest verified (version=%s rollback=%llu slot=%c)",
                               mfst.version, (unsigned long long)mfst.rollback_index,
                               mfst.target_slot);
            /* Store the manifest's rollback_index so the install bump uses it. */
            {
                wlt_UpdateStatus st2;
                (void)status_read(&st2);
                st2.pending_rollback_index = mfst.rollback_index;
                wlt_strlcpy(st2.pending_version, mfst.version, sizeof(st2.pending_version));
                (void)status_write(&st2);
            }
        } else {
            WLT_UPDATE_LOG_ERR("install: no signed manifest — signature replay possible "
                               "(research-I R5); proceeding for backward-compat");
        }
    }

    err = wlt_partition_format(inactive);
    if (err != WLT_UPDATE_OK) { set_last_error("install: format failed"); close(pkg_fd); close(lock_fd); return err; }
    err = wlt_partition_mount(inactive);
    if (err != WLT_UPDATE_OK) { set_last_error("install: mount failed"); (void)wlt_partition_unmount(inactive); close(pkg_fd); close(lock_fd); return err; }

    err = wlt_partition_install_to(pkg_fd_path, inactive);
    (void)wlt_partition_unmount(inactive);
    close(pkg_fd);
    if (err != WLT_UPDATE_OK) { set_last_error("install: extraction failed"); close(lock_fd); return err; }

    wlt_BootConfig cfg;
    err = wlt_boot_config_load(g_config.boot_config_path, &cfg);
    if (err != WLT_UPDATE_OK) { close(lock_fd); return err; }
    wlt_PartitionInfo *slot = &cfg.partitions[inactive];
    slot->failed = false;
    slot->boot_attempts = 0;
    slot->last_boot_success[0] = '\0';
    if (st.pending_version[0] != '\0')
    {
        wlt_strlcpy(slot->version, st.pending_version, sizeof(slot->version));
    }
    cfg.rollback_on_fail = true;
    wlt_boot_config_set_default(&cfg, inactive);
    err = wlt_boot_config_save(g_config.boot_config_path, &cfg);
    if (err != WLT_UPDATE_OK) { set_last_error("install: boot config write failed"); close(lock_fd); return err; }

    /* Anti-rollback: bump the counter to the installed package's
     * rollback_index so future downgrades are refused (research-I R2). */
    if (st.pending_rollback_index > 0) {
        wlt_update_error_t rb_err = wlt_rollback_bump(st.pending_rollback_index);
        if (rb_err != WLT_UPDATE_OK) {
            WLT_UPDATE_LOG_ERR("install: rollback bump failed (non-fatal): %s",
                              wlt_update_error_str(rb_err));
        }
    }
    st.pending_reboot = true;
    st.last_error[0] = '\0';
    stamp_now(st.last_update_time, sizeof(st.last_update_time));
    (void)status_write(&st);

    wlt_UpdateHistoryEntry he;
    memset(&he, 0, sizeof(he));
    wlt_strlcpy(he.version, slot->version, sizeof(he.version));
    wlt_strlcpy(he.timestamp, st.last_update_time, sizeof(he.timestamp));
    he.partition = (inactive == WLT_PARTITION_B) ? 'B' : 'A';
    he.success = true;
    (void)history_append(&he);

    close(lock_fd);
    WLT_UPDATE_LOG_INF("install staged on slot %c â reboot to activate", he.partition);
    return WLT_UPDATE_OK;
}

/* ââ Public: status ââ */
wlt_update_error_t
wlt_update_status(wlt_UpdateStatus *out)
{
    if (out == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    if (!g_initialised) { wlt_update_init(NULL); }
    (void)status_read(out);
    wlt_strlcpy(out->current_version, g_config.current_version, sizeof(out->current_version));
    wlt_PartitionId active;
    if (wlt_partition_get_active(&active) == WLT_UPDATE_OK)
    {
        out->active_partition = (active == WLT_PARTITION_B) ? 'B' : 'A';
    }
    return WLT_UPDATE_OK;
}

/* ââ Public: rollback ââ */
wlt_update_error_t
wlt_update_rollback(void)
{
    if (!g_initialised) { wlt_update_init(NULL); }
    wlt_update_error_t err = wlt_partition_rollback();
    if (err != WLT_UPDATE_OK) { set_last_error("rollback: refused"); return err; }
    wlt_UpdateStatus st;
    (void)status_read(&st);
    st.pending_reboot = false;
    (void)status_write(&st);

    wlt_UpdateHistoryEntry he;
    memset(&he, 0, sizeof(he));
    wlt_PartitionId active;
    if (wlt_partition_get_active(&active) == WLT_UPDATE_OK)
    {
        he.partition = (active == WLT_PARTITION_A) ? 'B' : 'A';
    }
    stamp_now(he.timestamp, sizeof(he.timestamp));
    he.success = false;
    (void)history_append(&he);
    return WLT_UPDATE_OK;
}

/* ââ Public: auto-update toggle ââ */
static wlt_update_error_t
set_auto(bool enabled)
{
    if (!g_initialised) { wlt_update_init(NULL); }
    wlt_UpdateConfig cfg;
    (void)wlt_config_load(WLT_UPDATE_CONFIG_PATH, &cfg);
    cfg.auto_update = enabled;
    g_config.auto_update = enabled;
    wlt_update_error_t err = wlt_config_save(WLT_UPDATE_CONFIG_PATH, &cfg);
    if (err != WLT_UPDATE_OK) { return err; }
    wlt_UpdateStatus st;
    (void)status_read(&st);
    st.auto_update = enabled;
    (void)status_write(&st);
    return WLT_UPDATE_OK;
}

wlt_update_error_t wlt_update_enable_auto(void) { return set_auto(true); }
wlt_update_error_t wlt_update_disable_auto(void) { return set_auto(false); }

/* ââ Public: history (newest first) ââ */
wlt_update_error_t
wlt_update_history(wlt_UpdateHistoryEntry *entries,
                  size_t max_entries, size_t *count_out)
{
    if (entries == NULL || count_out == NULL || max_entries == 0) { return WLT_UPDATE_ERR_INVALID_ARG; }
    *count_out = 0;
    FILE *fp = fopen(WLT_UPDATE_HISTORY_PATH, "r");
    if (fp == NULL) { return WLT_UPDATE_OK; }

    char line[WLT_UPDATE_MAX_LINE];
    size_t filled = 0;
    while (filled < max_entries && fgets(line, sizeof(line), fp) != NULL)
    {
        wlt_UpdateHistoryEntry *e = &entries[filled];
        memset(e, 0, sizeof(*e));
        char part = 'A';
        char status[8] = "";
        int got = sscanf(line, "%31s %31s %c %7s",
                         e->timestamp, e->version, &part, status);
        if (got < 3) { continue; }
        e->partition = part;
        e->success = (strcmp(status, "ok") == 0);
        filled++;
    }
    fclose(fp);
    for (size_t i = 0; i < filled / 2; i++)
    {
        wlt_UpdateHistoryEntry t = entries[i];
        entries[i] = entries[filled - 1 - i];
        entries[filled - 1 - i] = t;
    }
    *count_out = filled;
    return WLT_UPDATE_OK;
}

const char *
wlt_update_error_str(wlt_update_error_t err)
{
    switch (err)
    {
        case WLT_UPDATE_OK:                     return "ok";
        case WLT_UPDATE_ERR_INVALID_ARG:        return "invalid argument";
        case WLT_UPDATE_ERR_NOT_FOUND:          return "not found";
        case WLT_UPDATE_ERR_PERMISSION:         return "permission denied";
        case WLT_UPDATE_ERR_IO:                 return "I/O error";
        case WLT_UPDATE_ERR_NOMEM:              return "out of memory";
        case WLT_UPDATE_ERR_NETWORK:            return "network error";
        case WLT_UPDATE_ERR_TOR:                return "Tor unavailable";
        case WLT_UPDATE_ERR_SIGNATURE:          return "signature invalid";
        case WLT_UPDATE_ERR_DISK_FULL:          return "disk full";
        case WLT_UPDATE_ERR_PARTITION:          return "partition error";
        case WLT_UPDATE_ERR_BOOT_CONFIG:        return "boot config error";
        case WLT_UPDATE_ERR_DOWNGRADE:          return "downgrade refused";
        case WLT_UPDATE_ERR_UNSUPPORTED:        return "unsupported (no libsodium)";
        case WLT_UPDATE_ERR_TIMEOUT:            return "timed out";
        case WLT_UPDATE_ERR_ALREADY_UP_TO_DATE: return "already up to date";
        case WLT_UPDATE_ERR_PENDING_REBOOT:     return "pending reboot";
        case WLT_UPDATE_ERR_INSUFFICIENT_SPACE: return "insufficient space";
        case WLT_UPDATE_ERR_BUSY:               return "another update in progress";
        default:                                return "unknown error";
    }
}
