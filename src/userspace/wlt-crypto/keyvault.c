/**
 * @file keyvault.c
 * @brief Key Vault IPC — STUB (seL4 PD communication not yet implemented)
 * @module wlt-crypto/keyvault
 * @purpose Placeholder for seL4 Key Vault PD communication
 * @dependencies: crypto.h, keyvault.h
 * @tested_by: tests/userspace/crypto_test.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * ARCHITECTURE NOTE (Claude's audit 2026-06-22):
 *
 * The previous implementation used Unix sockets (/run/key-vault.sock) to
 * communicate with the Key Vault PD. This is architecturally IMPOSSIBLE:
 * seL4 PDs are isolated from Linux and cannot expose Unix sockets.
 *
 * The correct IPC mechanism is seL4 Microkit channels (notification +
 * shared memory). Until the Microkit build is working, this module is a
 * STUB that returns WLT_CRYPTO_ERR_UNSUPPORTED.
 */

#include "crypto.h"
#include "keyvault.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

wlt_crypto_error_t
wlt_keyvault_init(void)
{
    /* TODO: Implement seL4 Microkit channel setup */
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

void
wlt_keyvault_shutdown(void)
{
    /* TODO: Close seL4 notification channel */
}

wlt_crypto_error_t
wlt_keyvault_ping(void)
{
    /* TODO: Send ping via seL4 notification, wait for response */
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_encrypt(const char *key_id,
                     const uint8_t *plaintext, size_t plaintext_len,
                     uint8_t *ciphertext, size_t *ciphertext_len)
{
    (void)key_id; (void)plaintext; (void)plaintext_len;
    (void)ciphertext; (void)ciphertext_len;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_decrypt(const char *key_id,
                     const uint8_t *ciphertext, size_t ciphertext_len,
                     uint8_t *plaintext, size_t *plaintext_len)
{
    (void)key_id; (void)ciphertext; (void)ciphertext_len;
    (void)plaintext; (void)plaintext_len;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_generate_key(wlt_kv_key_type_t type,
                          char *key_id_out, size_t key_id_size)
{
    (void)type; (void)key_id_out; (void)key_id_size;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_delete_key(const char *key_id)
{
    (void)key_id;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_list_keys(char *buf, size_t buf_size, size_t *written)
{
    (void)buf; (void)buf_size;
    if (written) *written = 0;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_get_volume_key(const char *key_id,
                            uint8_t *key_out, size_t *key_len)
{
    (void)key_id; (void)key_out; (void)key_len;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

/* ─── Bridge integration (research-H) ───────────────────────────
 * wlt_keyvault_get_volume_key_fd: connect to the wlt-keyvault-bridge
 * daemon at /run/wlt-keyvault.sock, request the volume key for key_id,
 * and return it as a pipe read-end fd (64 bytes).
 *
 * Architecture (per research-H):
 *   Linux guest  <->  /run/wlt-keyvault.sock (AF_UNIX)
 *                        |
 *                 wlt-keyvault-bridge daemon (reads from /dev/virtio-port/wltkv)
 *                        |
 *                 seL4 VMM PD  <->  Microkit channel  <->  KeyVaultPD
 *
 * The bridge daemon is a separate userspace process (not yet implemented —
 * it needs the seL4 KeyVaultPD + virtio-console integration, which is a
 * hardware/QEMU-gated task). This function is the client side: it talks
 * to whatever is listening on the socket. When the bridge isn't running,
 * it returns WLT_CRYPTO_ERR_KEYVAULT (fail-closed).
 *
 * Wire protocol (simple, length-prefixed):
 *   Request:  [u32 op=7 GET_VOLUME_KEY] [u32 key_id_len] [key_id bytes]
 *   Response: [i32 status] [u32 key_len] [key bytes]
 * (Matches the wlt_kv_op_t protocol in keyvault.h.) */
#define WLT_KV_BRIDGE_SOCKET "/run/wlt-keyvault.sock"
#define WLT_KV_KEY_BYTES     64

wlt_crypto_error_t
wlt_keyvault_get_volume_key_fd(const char *key_id, int *fd_out)
{
    if (key_id == NULL || fd_out == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    *fd_out = -1;

    int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sock < 0)
        return WLT_CRYPTO_ERR_KEYVAULT;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(WLT_KV_BRIDGE_SOCKET) >= sizeof(addr.sun_path)) {
        close(sock);
        return WLT_CRYPTO_ERR_TOO_LARGE;
    }
    strncpy(addr.sun_path, WLT_KV_BRIDGE_SOCKET, sizeof(addr.sun_path) - 1);

    /* 2s connect timeout — the bridge is loopback; if it's not up, fail fast. */
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        /* Bridge not running — fail CLOSED. Never fall back to a hardcoded key. */
        close(sock);
        return WLT_CRYPTO_ERR_KEYVAULT;
    }

    /* Send the request: op + key_id_len + key_id. */
    uint32_t key_id_len = (uint32_t)strlen(key_id);
    uint32_t op = WLT_KV_OP_GET_VOLUME_KEY;
    /* Send op + key_id_len in one write, then key_id. */
    uint8_t hdr[8];
    hdr[0] = op & 0xff; hdr[1] = (op >> 8) & 0xff;
    hdr[2] = (op >> 16) & 0xff; hdr[3] = (op >> 24) & 0xff;
    hdr[4] = key_id_len & 0xff; hdr[5] = (key_id_len >> 8) & 0xff;
    hdr[6] = (key_id_len >> 16) & 0xff; hdr[7] = (key_id_len >> 24) & 0xff;
    if (write(sock, hdr, 8) != 8 || write(sock, key_id, key_id_len) != (ssize_t)key_id_len) {
        close(sock);
        return WLT_CRYPTO_ERR_IO;
    }

    /* Read the response: status (4 bytes, LE) + key_len (4 bytes, LE). */
    uint8_t resp_hdr[8];
    ssize_t total = 0;
    while (total < 8) {
        ssize_t r = read(sock, resp_hdr + total, 8 - total);
        if (r <= 0) { close(sock); return WLT_CRYPTO_ERR_IO; }
        total += r;
    }
    int32_t status = (int32_t)(resp_hdr[0] | (resp_hdr[1] << 8) |
                               (resp_hdr[2] << 16) | (resp_hdr[3] << 24));
    uint32_t key_len = (uint32_t)(resp_hdr[4] | (resp_hdr[5] << 8) |
                                  (resp_hdr[6] << 16) | (resp_hdr[7] << 24));
    if (status != 0) {
        close(sock);
        return WLT_CRYPTO_ERR_KEYVAULT;
    }
    if (key_len != WLT_KV_KEY_BYTES) {
        close(sock);
        return WLT_CRYPTO_ERR_INVALID_ARG;
    }

    /* Create a pipe; read the key bytes from the socket into the pipe
     * write-end, then close the socket + write-end. The caller (luks.c)
     * reads from the pipe read-end. */
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) < 0) {
        close(sock);
        return WLT_CRYPTO_ERR_IO;
    }

    /* Read the key from the socket into a buffer, then write it to the pipe. */
    uint8_t keybuf[WLT_KV_KEY_BYTES];
    total = 0;
    while ((size_t)total < sizeof(keybuf)) {
        ssize_t r = read(sock, keybuf + total, sizeof(keybuf) - total);
        if (r <= 0) {
            close(sock); close(pipefd[0]); close(pipefd[1]);
            return WLT_CRYPTO_ERR_IO;
        }
        total += r;
    }
    close(sock);

    /* Write the key to the pipe + close the write end so the caller gets EOF. */
    ssize_t w = write(pipefd[1], keybuf, sizeof(keybuf));
    /* Zero the buffer immediately. */
    volatile uint8_t *p = (volatile uint8_t *)keybuf;
    for (size_t i = 0; i < sizeof(keybuf); i++) p[i] = 0;
    close(pipefd[1]);
    if (w != sizeof(keybuf)) {
        close(pipefd[0]);
        return WLT_CRYPTO_ERR_IO;
    }

    *fd_out = pipefd[0];
    return WLT_CRYPTO_OK;
}
