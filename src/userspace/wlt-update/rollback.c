/**
 * @file rollback.c
 * @brief Anti-rollback counter with HMAC-SHA256 integrity (libsodium)
 *
 * MVP implementation per research-I R2. The HMAC key is a build-time
 * constant; the full design uses TPM2 NV or seL4-owned reserved flash
 * (documented honestly in the header + research-I).
 */
#include "rollback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_SODIUM
#include <sodium.h>

/* Build-time HMAC key. MARKED AS NEEDING KeyVaultPD DELIVERY.
 *
 * This is a compromise: it prevents an attacker who can write the
 * counter file (e.g. via a filesystem exploit) from trivially lowering
 * the counter, because they can't forge the HMAC without the key. It
 * does NOT prevent an attacker with root + the binary (who can extract
 * the key from the .rodata section). The full fix is tamper-evident
 * hardware (TPM2 NV counter) or a seL4-owned reserved flash region
 * accessible only via a capability-gated Microkit channel. See
 * research-I R2 + research-H in the worklog. */
static const uint8_t ROLLBACK_HMAC_KEY[32] = {
    0x57, 0x4c, 0x54, 0x49, 0x4f, 0x53, 0x2d, 0x72,  /* "WLTIOS-r" */
    0x6f, 0x6c, 0x6c, 0x62, 0x61, 0x63, 0x6b, 0x2d,  /* "ollback-" */
    0x6b, 0x65, 0x79, 0x2d, 0x76, 0x31, 0x00, 0x00,  /* "key-v1"   */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   /* pad        */
};

/* HMAC-SHA256 produces 32 bytes; base64 of 32 bytes = 44 chars (with pad).
 * We store it as 64 hex chars instead for simpler parsing. */
#define HMAC_HEX_LEN 64

static void
to_hex(const uint8_t *bin, size_t len, char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex[(bin[i] >> 4) & 0xf];
        out[i * 2 + 1] = hex[bin[i] & 0xf];
    }
    out[len * 2] = '\0';
}

/* Compute HMAC-SHA256 over the counter's decimal string representation. */
static void
compute_hmac(uint64_t counter, char hex_out[HMAC_HEX_LEN + 1])
{
    char numstr[32];
    snprintf(numstr, sizeof(numstr), "%llu", (unsigned long long)counter);
    uint8_t mac[crypto_auth_BYTES];  /* 32 bytes for HMAC-SHA256 */
    crypto_auth(mac, (const unsigned char *)numstr, strlen(numstr),
                ROLLBACK_HMAC_KEY);
    to_hex(mac, sizeof(mac), hex_out);
}

static int
verify_hmac(uint64_t counter, const char *hex_mac)
{
    char expected[HMAC_HEX_LEN + 1];
    compute_hmac(counter, expected);
    return sodium_memcmp(expected, hex_mac, HMAC_HEX_LEN) == 0;
}

#endif /* HAVE_SODIUM */

wlt_update_error_t
wlt_rollback_load(uint64_t *out_counter)
{
    if (out_counter == NULL)
        return WLT_UPDATE_ERR_INVALID_ARG;
    *out_counter = 0;  /* default: no counter = first install allowed */

    FILE *fp = fopen(WLT_UPDATE_ROLLBACK_PATH, "r");
    if (fp == NULL) {
        /* File doesn't exist yet — this is the first boot or a fresh install.
         * Counter = 0, any package with rollback_index >= 0 is allowed. */
        return WLT_UPDATE_OK;
    }

    uint64_t counter = 0;
    char hmac_line[HMAC_HEX_LEN + 2] = {0};  /* hex + newline + nul;
        [fix] zero-initialized - if fgets() below fails (e.g. a
        truncated counter file with only one line), this stays a
        defined empty string instead of leaving strlen() below to
        read indeterminate stack memory as a C string. */
    char line[128];

    if (fgets(line, sizeof(line), fp) != NULL) {
        counter = strtoull(line, NULL, 10);
    }
    if (fgets(hmac_line, sizeof(hmac_line), fp) != NULL) {
        /* strip newline */
        hmac_line[strcspn(hmac_line, "\r\n")] = '\0';
    }
    fclose(fp);

#ifdef HAVE_SODIUM
    if (strlen(hmac_line) == HMAC_HEX_LEN) {
        if (!verify_hmac(counter, hmac_line)) {
            /* TAMPER DETECTED: the counter file's HMAC doesn't match.
             * Fail-closed: refuse to proceed rather than silently resetting
             * to 0 (which would allow a downgrade attack). */
            WLT_UPDATE_LOG_ERR("rollback: counter file HMAC mismatch "
                               "(tamper-evident refusal)");
            return WLT_UPDATE_ERR_IO;
        }
    } else {
        /* No HMAC line — either a legacy file or corruption. Fail-closed. */
        WLT_UPDATE_LOG_ERR("rollback: counter file missing HMAC");
        return WLT_UPDATE_ERR_IO;
    }
#else
    /* Built without libsodium: can't verify HMAC. Allow the load but log
     * a warning — the counter is best-effort, not tamper-evident. */
    WLT_UPDATE_LOG_ERR("rollback: built without libsodium, HMAC not verified");
#endif

    *out_counter = counter;
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_rollback_store(uint64_t counter)
{
    /* Atomic write: temp file + fsync + rename + dir fsync. */
    char tmp[WLT_UPDATE_MAX_PATH + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", WLT_UPDATE_ROLLBACK_PATH);

    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        if (errno == EEXIST) {
            /* Stale temp from a previous crash — remove and retry once. */
            unlink(tmp);
            fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        }
        if (fd < 0) {
            WLT_UPDATE_LOG_ERR("rollback: cannot create temp: %s",
                               strerror(errno));
            return WLT_UPDATE_ERR_IO;
        }
    }

    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }

    fprintf(fp, "%llu\n", (unsigned long long)counter);

#ifdef HAVE_SODIUM
    char hmac[HMAC_HEX_LEN + 1];
    compute_hmac(counter, hmac);
    fprintf(fp, "%s\n", hmac);
#endif

    if (fflush(fp) != 0 || fsync(fileno(fp)) < 0) {
        fclose(fp);
        unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }
    fclose(fp);

    if (rename(tmp, WLT_UPDATE_ROLLBACK_PATH) < 0) {
        unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }

    /* fsync the parent directory so the rename is durable. */
    const char *dir = strrchr(WLT_UPDATE_ROLLBACK_PATH, '/');
    if (dir) {
        char dirpath[WLT_UPDATE_MAX_PATH];
        size_t dlen = (size_t)(dir - WLT_UPDATE_ROLLBACK_PATH);
        if (dlen < sizeof(dirpath)) {
            memcpy(dirpath, WLT_UPDATE_ROLLBACK_PATH, dlen);
            dirpath[dlen] = '\0';
            int dfd = open(dirpath, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (dfd >= 0) {
                fsync(dfd);
                close(dfd);
            }
        }
    }

    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_rollback_check(uint64_t package_index)
{
    uint64_t stored = 0;
    wlt_update_error_t err = wlt_rollback_load(&stored);
    if (err != WLT_UPDATE_OK)
        return err;

    if (package_index < stored) {
        WLT_UPDATE_LOG_ERR("rollback: refusing downgrade "
                           "(package index %llu < stored %llu)",
                           (unsigned long long)package_index,
                           (unsigned long long)stored);
        return WLT_UPDATE_ERR_DOWNGRADE;
    }
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_rollback_bump(uint64_t package_index)
{
    uint64_t stored = 0;
    wlt_update_error_t err = wlt_rollback_load(&stored);
    if (err != WLT_UPDATE_OK)
        return err;

    if (package_index > stored) {
        return wlt_rollback_store(package_index);
    }
    return WLT_UPDATE_OK;  /* no bump needed */
}
