/**
 * @file keyvault.c
 * @brief Unix socket IPC client for the seL4 Key Vault PD
 * @module wlt-crypto/keyvault
 * @purpose Connects to /run/key-vault.sock, frames requests per the
 *          wire protocol in keyvault.h, enforces a per-operation
 *          timeout, and retries transient PING failures. Plaintext
 *          buffers returned from decrypt are zeroed by the caller;
 *          this module additionally zeroes internal staging buffers
 *          and the volume-key buffer on failure.
 * @dependencies: crypto.h, keyvault.h, libsodium (optional)
 * @tested_by: tests/userspace/keyvault_test.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 */

#include "crypto.h"
#include "keyvault.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/select.h>

#ifdef HAVE_SODIUM
#include <sodium.h>
#endif

/* ─── Module State ───────────────────────────────────── */

static int  g_kv_fd = -1;
static bool g_kv_initialized = false;

/* ─── Internal: Byte Order + Zeroing ───────────────────
 * Wire integers are little-endian, built byte-by-byte to avoid any
 * host-endianness assumption.
 */

static void
put_u32_le(uint8_t buf[4], uint32_t v)
{
    buf[0] = (uint8_t)(v & 0xff);
    buf[1] = (uint8_t)((v >> 8) & 0xff);
    buf[2] = (uint8_t)((v >> 16) & 0xff);
    buf[3] = (uint8_t)((v >> 24) & 0xff);
}

static uint32_t
get_u32_le(const uint8_t buf[4])
{
    return  (uint32_t)buf[0]
          | ((uint32_t)buf[1] << 8)
          | ((uint32_t)buf[2] << 16)
          | ((uint32_t)buf[3] << 24);
}

static int32_t
get_i32_le(const uint8_t buf[4])
{
    return (int32_t)get_u32_le(buf);
}

static void
zero_buf(void *buf, size_t len)
{
    if (buf == NULL || len == 0)
        return;
#ifdef HAVE_SODIUM
    sodium_memzero(buf, len);
#else
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len--)
        *p++ = 0;
#endif
}

/* ─── Internal: Bounded Socket I/O ─────────────────────
 * io_xfer transfers exactly n bytes or returns -errno. A select()
 * poll enforces the timeout so a wedged PD cannot stall the caller.
 * write_all / read_all are thin direction-typed wrappers.
 */

static int
io_xfer(int fd, void *buf, size_t n, int timeout_ms, bool is_write)
{
    uint8_t *p = (uint8_t *)buf;
    size_t off = 0;
    while (off < n)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int sel = select(fd + 1,
                         is_write ? NULL : &fds,
                         is_write ? &fds : NULL,
                         NULL, &tv);
        if (sel < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (sel == 0)
            return -ETIMEDOUT;

        ssize_t r = is_write ? write(fd, p + off, n - off)
                             : read(fd, p + off, n - off);
        if (r < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        if (!is_write && r == 0)
            return -ECONNRESET;  /* server closed early */
        off += (size_t)r;
    }
    return 0;
}

static int
write_all(int fd, const void *buf, size_t n, int timeout_ms)
{
    return io_xfer(fd, (void *)buf, n, timeout_ms, true);
}

static int
read_all(int fd, void *buf, size_t n, int timeout_ms)
{
    return io_xfer(fd, buf, n, timeout_ms, false);
}

/* ─── Internal: Connection Management ────────────────── */

static void
drop_connection(void)
{
    if (g_kv_fd >= 0)
    {
        close(g_kv_fd);
        g_kv_fd = -1;
    }
}

static wlt_crypto_error_t
connect_to_pd(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        WLT_CRYPTO_LOG_ERR("kv: socket() failed: %s", strerror(errno));
        return WLT_CRYPTO_ERR_IO;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WLT_KEYVAULT_SOCKET, sizeof(addr.sun_path) - 1);

    /* The socket file may not exist yet (PD still starting). Retry a
     * few times with a short sleep before giving up. */
    int tries = 0;
    int rc = -1;
    while (tries < WLT_KEYVAULT_CONNECT_TRIES)
    {
        rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (rc == 0)
            break;
        if (errno != ENOENT && errno != ECONNREFUSED)
            break;
        tries++;
        struct timespec ts = {0, 200 * 1000 * 1000};  /* 200 ms */
        nanosleep(&ts, NULL);
    }

    if (rc < 0)
    {
        WLT_CRYPTO_LOG_ERR("kv: connect(%s) failed after %d tries: %s",
                           WLT_KEYVAULT_SOCKET, tries, strerror(errno));
        close(fd);
        return WLT_CRYPTO_ERR_KEYVAULT;
    }

    g_kv_fd = fd;
    return WLT_CRYPTO_OK;
}

/* ─── Internal: Wire Framing ───────────────────────────
 * send_request emits [op][key_id_len][key_id][payload_len][payload].
 * recv_response reads [status][resp_len][resp] and bounds-checks the
 * announced length against the caller's buffer. Both return 0 on
 * success or -errno on I/O failure.
 */

static int
send_request(int fd, wlt_kv_op_t op, const char *key_id,
             const uint8_t *payload, size_t payload_len, int timeout_ms)
{
    size_t key_id_len = (key_id != NULL) ? strlen(key_id) : 0;

    uint8_t hdr[8];
    put_u32_le(hdr + 0, (uint32_t)op);
    put_u32_le(hdr + 4, (uint32_t)key_id_len);
    int rc = write_all(fd, hdr, 8, timeout_ms);
    if (rc < 0)
        return rc;

    if (key_id_len > 0)
    {
        rc = write_all(fd, key_id, key_id_len, timeout_ms);
        if (rc < 0)
            return rc;
    }

    uint8_t plen_buf[4];
    put_u32_le(plen_buf, (uint32_t)payload_len);
    rc = write_all(fd, plen_buf, 4, timeout_ms);
    if (rc < 0)
        return rc;

    if (payload_len > 0)
        rc = write_all(fd, payload, payload_len, timeout_ms);
    return rc;
}

static int
recv_response(int fd, int32_t *status_out,
              uint8_t *resp_buf, size_t resp_cap, size_t *resp_len_out,
              int timeout_ms)
{
    uint8_t rhdr[8];
    int rc = read_all(fd, rhdr, 8, timeout_ms);
    if (rc < 0)
        return rc;

    int32_t status = get_i32_le(rhdr);
    uint32_t rlen = get_u32_le(rhdr + 4);
    if (rlen > WLT_KEYVAULT_MAX_PAYLOAD)
    {
        WLT_CRYPTO_LOG_ERR("kv: response too large (%u bytes)", rlen);
        return -EMSGSIZE;
    }
    if (rlen > resp_cap)
    {
        WLT_CRYPTO_LOG_ERR("kv: response buffer too small (%u > %zu)",
                           rlen, resp_cap);
        return -EMSGSIZE;
    }

    if (rlen > 0)
    {
        rc = read_all(fd, resp_buf, rlen, timeout_ms);
        if (rc < 0)
            return rc;
    }

    *status_out = status;
    *resp_len_out = rlen;
    return 0;
}

/* ─── Internal: Request/Response Exchange ──────────────
 * Sends a request and reads the response. On I/O error the connection
 * is dropped (it is in an indeterminate state). When allow_retry is
 * set, a single reconnect+retry is attempted — used only for PING,
 * since encrypt/decrypt are not idempotent and must be retried by the
 * caller at the chunk level.
 */

static wlt_crypto_error_t
io_err_to_wlt(int rc)
{
    return (rc == -ETIMEDOUT) ? WLT_CRYPTO_ERR_TIMEOUT
                              : WLT_CRYPTO_ERR_IO;
}

static wlt_crypto_error_t
exchange(wlt_kv_op_t op, const char *key_id,
         const uint8_t *payload, size_t payload_len,
         uint8_t *resp_buf, size_t resp_cap, size_t *resp_len,
         bool allow_retry)
{
    if (g_kv_fd < 0)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (payload_len > WLT_KEYVAULT_MAX_PAYLOAD)
        return WLT_CRYPTO_ERR_TOO_LARGE;
    if (key_id != NULL && strlen(key_id) > WLT_CRYPTO_MAX_KEY_ID)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    for (int attempt = 0; attempt <= (allow_retry ? 1 : 0); attempt++)
    {
        int rc = send_request(g_kv_fd, op, key_id, payload, payload_len,
                              WLT_KEYVAULT_TIMEOUT_MS);
        if (rc < 0)
        {
            WLT_CRYPTO_LOG_ERR("kv: send error: %s", strerror(-rc));
            drop_connection();
            if (attempt < 1 && connect_to_pd() == WLT_CRYPTO_OK)
                continue;
            return io_err_to_wlt(rc);
        }

        int32_t status = 0;
        size_t rlen = 0;
        rc = recv_response(g_kv_fd, &status, resp_buf, resp_cap,
                           &rlen, WLT_KEYVAULT_TIMEOUT_MS);
        if (rc < 0)
        {
            WLT_CRYPTO_LOG_ERR("kv: recv error: %s", strerror(-rc));
            drop_connection();
            if (attempt < 1 && connect_to_pd() == WLT_CRYPTO_OK)
                continue;
            return io_err_to_wlt(rc);
        }

        *resp_len = rlen;
        /* The PD uses the same numeric scheme as wlt_crypto_error_t. */
        return (wlt_crypto_error_t)status;
    }
    return WLT_CRYPTO_ERR_TIMEOUT;
}

/* ─── Lifecycle ──────────────────────────────────────── */

wlt_crypto_error_t
wlt_keyvault_init(void)
{
    if (g_kv_initialized)
        return WLT_CRYPTO_OK;
    wlt_crypto_error_t err = connect_to_pd();
    if (err != WLT_CRYPTO_OK)
        return err;
    g_kv_initialized = true;
    return WLT_CRYPTO_OK;
}

void
wlt_keyvault_shutdown(void)
{
    drop_connection();
    g_kv_initialized = false;
}

wlt_crypto_error_t
wlt_keyvault_ping(void)
{
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    uint8_t dummy;
    size_t rlen = 0;
    return exchange(WLT_KV_OP_PING, NULL, NULL, 0,
                    &dummy, 0, &rlen, true);
}

/* ─── Crypto Operations ──────────────────────────────── */

wlt_crypto_error_t
wlt_keyvault_encrypt(const char *key_id,
                     const uint8_t *plaintext, size_t plaintext_len,
                     uint8_t *ciphertext, size_t *ciphertext_len)
{
    if (key_id == NULL || ciphertext == NULL || ciphertext_len == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (plaintext_len > 0 && plaintext == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (plaintext_len > WLT_KEYVAULT_MAX_PAYLOAD)
        return WLT_CRYPTO_ERR_TOO_LARGE;
    /* The PD returns nonce(24) + MAC(16) + ciphertext. */
    if (*ciphertext_len < plaintext_len + WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    size_t rlen = 0;
    wlt_crypto_error_t err = exchange(WLT_KV_OP_ENCRYPT, key_id,
                                      plaintext, plaintext_len,
                                      ciphertext, *ciphertext_len,
                                      &rlen, false);
    if (err == WLT_CRYPTO_OK)
        *ciphertext_len = rlen;
    return err;
}

wlt_crypto_error_t
wlt_keyvault_decrypt(const char *key_id,
                     const uint8_t *ciphertext, size_t ciphertext_len,
                     uint8_t *plaintext, size_t *plaintext_len)
{
    if (key_id == NULL || ciphertext == NULL ||
        plaintext == NULL || plaintext_len == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;

    size_t rlen = 0;
    wlt_crypto_error_t err = exchange(WLT_KV_OP_DECRYPT, key_id,
                                      ciphertext, ciphertext_len,
                                      plaintext, *plaintext_len,
                                      &rlen, false);
    if (err == WLT_CRYPTO_OK)
        *plaintext_len = rlen;
    else
        zero_buf(plaintext, *plaintext_len);  /* never leak partial output */
    return err;
}

wlt_crypto_error_t
wlt_keyvault_generate_key(wlt_kv_key_type_t type,
                          char *key_id_out, size_t key_id_size)
{
    if (key_id_out == NULL || key_id_size < WLT_CRYPTO_MAX_KEY_ID)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;

    uint8_t type_byte = (uint8_t)type;
    size_t rlen = 0;
    wlt_crypto_error_t err = exchange(WLT_KV_OP_GENERATE_KEY, NULL,
                                      &type_byte, 1,
                                      (uint8_t *)key_id_out,
                                      key_id_size - 1, &rlen, false);
    if (err == WLT_CRYPTO_OK)
        key_id_out[rlen] = '\0';
    return err;
}

wlt_crypto_error_t
wlt_keyvault_delete_key(const char *key_id)
{
    if (key_id == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    uint8_t dummy;
    size_t rlen = 0;
    return exchange(WLT_KV_OP_DELETE_KEY, key_id,
                    NULL, 0, &dummy, 0, &rlen, false);
}

wlt_crypto_error_t
wlt_keyvault_list_keys(char *buf, size_t buf_size, size_t *written)
{
    if (buf == NULL || buf_size == 0 || written == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;

    *written = 0;
    size_t rlen = 0;
    wlt_crypto_error_t err = exchange(WLT_KV_OP_LIST_KEYS, NULL,
                                      NULL, 0,
                                      (uint8_t *)buf, buf_size - 1,
                                      &rlen, false);
    if (err == WLT_CRYPTO_OK)
    {
        buf[rlen] = '\0';
        *written = rlen;
    }
    return err;
}

wlt_crypto_error_t
wlt_keyvault_get_volume_key(const char *key_id,
                            uint8_t *key_out, size_t *key_len)
{
    if (key_id == NULL || key_out == NULL || key_len == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!g_kv_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (*key_len < WLT_KEYVAULT_VOLUME_KEY_LEN)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    size_t rlen = 0;
    wlt_crypto_error_t err = exchange(WLT_KV_OP_GET_VOLUME_KEY, key_id,
                                      NULL, 0, key_out, *key_len,
                                      &rlen, false);
    if (err == WLT_CRYPTO_OK)
        *key_len = rlen;
    else
        zero_buf(key_out, *key_len);  /* never leak partial key material */
    return err;
}
