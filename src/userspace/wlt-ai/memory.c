/*
 * memory.c - WLT-AI Encrypted Conversation History
 *
 * Manages the AI assistant's conversation memory. Supports two modes:
 *
 *   - Persistent: History is encrypted and saved to disk, surviving
 *     reboots. The encryption key is derived from the user's key vault.
 *
 *   - Amnesic:    History is kept in memory only and wiped on exit.
 *     Nothing is written to disk.
 *
 * Encryption uses a XOR-stream cipher derived from the key via a
 * simple PRNG (LCG). This is suitable for at-rest protection of
 * conversation data within the sandbox; the key itself is never
 * stored on disk. (Production builds should swap in AES-256-GCM.)
 *
 * Copyright (c) WLTIOS Project. All rights reserved.
 */

#include "ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Internal state                                                     */
/* ------------------------------------------------------------------ */

static struct {
    char        path[WLT_AI_FILE_PATH_LEN];
    uint8_t     key[WLT_AI_ENCRYPT_KEY_LEN];
    size_t      key_len;
    bool        initialized;
} wlt_mem_state;

/* ------------------------------------------------------------------ */
/* Encryption: XOR stream cipher with LCG-seeded keystream            */
/* ------------------------------------------------------------------ */

/*
 * Derive a keystream from the encryption key using a Linear
 * Congruential Generator. For each byte of plaintext, we produce
 * a pseudo-random byte from the key material and XOR them.
 *
 * This is deliberately simple and dependency-free. The format on
 * disk is:
 *   [4-byte magic][4-byte version][4-byte payload_len][encrypted data]
 */

#define WLT_MEM_MAGIC       0x574C5441  /* "WLTA" */
#define WLT_MEM_VERSION     1

typedef struct {
    uint32_t    magic;
    uint32_t    version;
    uint32_t    payload_len;
} wlt_mem_header_t;

/* LCG state for keystream generation */
typedef struct {
    uint64_t    state;
} wlt_lcg_t;

static void
wlt_lcg_init(wlt_lcg_t *lcg, const uint8_t *key, size_t key_len)
{
    size_t i;
    lcg->state = 0;
    for (i = 0; i < key_len; i++) {
        lcg->state = (lcg->state << 8) | key[i];
        lcg->state = lcg->state * 6364136223846793005ULL + 1442695040888963407ULL;
    }
    /* Extra mixing pass */
    for (i = 0; i < 4; i++) {
        lcg->state = lcg->state * 6364136223846793005ULL + 1442695040888963407ULL;
    }
}

static uint8_t
wlt_lcg_next(wlt_lcg_t *lcg)
{
    lcg->state = lcg->state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint8_t)(lcg->state >> 33);
}

static void
wlt_crypt(const uint8_t *key, size_t key_len,
          const uint8_t *input, uint8_t *output, size_t len)
{
    wlt_lcg_t lcg;
    size_t i;

    wlt_lcg_init(&lcg, key, key_len);
    for (i = 0; i < len; i++) {
        output[i] = input[i] ^ wlt_lcg_next(&lcg);
    }
}

/* ------------------------------------------------------------------ */
/* Serialization                                                      */
/*                                                                     */
/* History is serialized as a simple text format:                     */
/*   <role>\t<timestamp>\t<content>\n                                 */
/* per message. The entire blob is then encrypted.                    */
/* ------------------------------------------------------------------ */

static size_t
wlt_serialize_history(const wlt_ai_history_t *history,
                      char *buf, size_t buf_len)
{
    size_t offset = 0;
    size_t i;

    for (i = 0; i < history->count; i++) {
        const wlt_ai_message_t *msg = &history->messages[i];
        int n;

        n = snprintf(buf + offset, buf_len - offset,
                     "%d\t%ld\t%s\n",
                     (int)msg->role, (long)msg->timestamp,
                     msg->content);

        if (n < 0 || (size_t)n >= buf_len - offset)
            break;
        offset += (size_t)n;
    }

    return offset;
}

static void
wlt_deserialize_history(const char *buf, size_t buf_len,
                        wlt_ai_history_t *history)
{
    size_t pos = 0;

    history->count = 0;
    history->head = 0;

    while (pos < buf_len && history->count < WLT_AI_MAX_HISTORY) {
        wlt_ai_message_t *msg = &history->messages[history->count];
        int role;
        long ts;
        size_t content_start;
        size_t field;

        /* Parse role */
        role = 0;
        while (pos < buf_len && buf[pos] >= '0' && buf[pos] <= '9') {
            role = role * 10 + (buf[pos] - '0');
            pos++;
        }
        if (pos >= buf_len || buf[pos] != '\t')
            break;
        pos++; /* skip tab */
        msg->role = (wlt_ai_role_t)role;

        /* Parse timestamp */
        ts = 0;
        while (pos < buf_len && buf[pos] >= '0' && buf[pos] <= '9') {
            ts = ts * 10 + (buf[pos] - '0');
            pos++;
        }
        if (pos >= buf_len || buf[pos] != '\t')
            break;
        pos++; /* skip tab */
        msg->timestamp = (time_t)ts;

        /* Parse content (until newline) */
        content_start = pos;
        while (pos < buf_len && buf[pos] != '\n')
            pos++;
        field = pos - content_start;
        if (field >= WLT_AI_MAX_MSG_LEN)
            field = WLT_AI_MAX_MSG_LEN - 1;
        memcpy(msg->content, buf + content_start, field);
        msg->content[field] = '\0';

        if (pos < buf_len)
            pos++; /* skip newline */

        history->count++;
        history->head = history->count % WLT_AI_MAX_HISTORY;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

wlt_ai_err_t
wlt_memory_init(const char *path, const uint8_t *key, size_t key_len)
{
    if (path == NULL || key == NULL || key_len == 0)
        return WLT_AI_ERR_INVALID;

    memset(&wlt_mem_state, 0, sizeof(wlt_mem_state));

    strncpy(wlt_mem_state.path, path, sizeof(wlt_mem_state.path) - 1);
    wlt_mem_state.path[sizeof(wlt_mem_state.path) - 1] = '\0';

    if (key_len > WLT_AI_ENCRYPT_KEY_LEN)
        key_len = WLT_AI_ENCRYPT_KEY_LEN;

    memcpy(wlt_mem_state.key, key, key_len);
    wlt_mem_state.key_len = key_len;
    wlt_mem_state.initialized = true;

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_memory_save(const wlt_ai_history_t *history)
{
    char *plaintext = NULL;
    uint8_t *ciphertext = NULL;
    wlt_mem_header_t header;
    int fd = -1;
    size_t pt_len;
    wlt_ai_err_t ret = WLT_AI_OK;

    if (!wlt_mem_state.initialized || history == NULL)
        return WLT_AI_ERR_INVALID;

    /* Amnesic mode: don't write anything */
    if (wlt_mem_state.key_len == 0)
        return WLT_AI_OK;

    /* Allocate buffer for serialized history */
    plaintext = malloc(WLT_AI_MAX_HISTORY * (WLT_AI_MAX_MSG_LEN + 64));
    if (plaintext == NULL)
        return WLT_AI_ERR_NOMEM;

    pt_len = wlt_serialize_history(history, plaintext,
                                   WLT_AI_MAX_HISTORY * (WLT_AI_MAX_MSG_LEN + 64));
    if (pt_len == 0) {
        ret = WLT_AI_OK; /* Nothing to save */
        goto cleanup;
    }

    /* Encrypt */
    ciphertext = malloc(pt_len);
    if (ciphertext == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    wlt_crypt(wlt_mem_state.key, wlt_mem_state.key_len,
              (const uint8_t *)plaintext, ciphertext, pt_len);

    /* Write header + ciphertext to disk */
    fd = open(wlt_mem_state.path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        fprintf(stderr, "wlt-ai: cannot open memory file %s: %s\n",
                wlt_mem_state.path, strerror(errno));
        ret = WLT_AI_ERR_IO;
        goto cleanup;
    }

    header.magic = WLT_MEM_MAGIC;
    header.version = WLT_MEM_VERSION;
    header.payload_len = (uint32_t)pt_len;

    if (write(fd, &header, sizeof(header)) != (ssize_t)sizeof(header)) {
        ret = WLT_AI_ERR_IO;
        goto cleanup;
    }

    if (write(fd, ciphertext, pt_len) != (ssize_t)pt_len) {
        ret = WLT_AI_ERR_IO;
        goto cleanup;
    }

cleanup:
    if (fd >= 0)
        close(fd);
    if (plaintext) {
        memset(plaintext, 0, pt_len);
        free(plaintext);
    }
    if (ciphertext) {
        free(ciphertext);
    }
    return ret;
}

wlt_ai_err_t
wlt_memory_load(wlt_ai_history_t *history)
{
    int fd = -1;
    wlt_mem_header_t header;
    uint8_t *ciphertext = NULL;
    char *plaintext = NULL;
    size_t pt_len;
    wlt_ai_err_t ret = WLT_AI_OK;

    if (!wlt_mem_state.initialized || history == NULL)
        return WLT_AI_ERR_INVALID;

    if (wlt_mem_state.key_len == 0)
        return WLT_AI_OK;

    memset(history, 0, sizeof(*history));

    fd = open(wlt_mem_state.path, O_RDONLY);
    if (fd < 0) {
        /* No existing memory file â start fresh */
        return WLT_AI_OK;
    }

    /* Read and validate header */
    if (read(fd, &header, sizeof(header)) != (ssize_t)sizeof(header)) {
        fprintf(stderr, "wlt-ai: memory file too small\n");
        ret = WLT_AI_ERR_ENCRYPT;
        goto cleanup;
    }

    if (header.magic != WLT_MEM_MAGIC) {
        fprintf(stderr, "wlt-ai: memory file bad magic\n");
        ret = WLT_AI_ERR_ENCRYPT;
        goto cleanup;
    }

    if (header.version != WLT_MEM_VERSION) {
        fprintf(stderr, "wlt-ai: memory file version mismatch\n");
        ret = WLT_AI_ERR_ENCRYPT;
        goto cleanup;
    }

    if (header.payload_len == 0 ||
        header.payload_len > WLT_AI_MAX_HISTORY * (WLT_AI_MAX_MSG_LEN + 64)) {
        fprintf(stderr, "wlt-ai: memory file invalid size %u\n",
                header.payload_len);
        ret = WLT_AI_ERR_ENCRYPT;
        goto cleanup;
    }

    /* Read ciphertext */
    ciphertext = malloc(header.payload_len);
    if (ciphertext == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    if (read(fd, ciphertext, header.payload_len) !=
        (ssize_t)header.payload_len) {
        ret = WLT_AI_ERR_IO;
        goto cleanup;
    }

    /* Decrypt */
    pt_len = header.payload_len;
    plaintext = malloc(pt_len + 1);
    if (plaintext == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    wlt_crypt(wlt_mem_state.key, wlt_mem_state.key_len,
              ciphertext, (uint8_t *)plaintext, pt_len);
    plaintext[pt_len] = '\0';

    /* Deserialize into history struct */
    wlt_deserialize_history(plaintext, pt_len, history);

cleanup:
    if (fd >= 0)
        close(fd);
    if (ciphertext)
        free(ciphertext);
    if (plaintext) {
        memset(plaintext, 0, pt_len + 1);
        free(plaintext);
    }
    return ret;
}

wlt_ai_err_t
wlt_memory_clear(void)
{
    if (!wlt_mem_state.initialized)
        return WLT_AI_ERR_INVALID;

    /* Remove the memory file from disk */
    if (wlt_mem_state.path[0] != '\0') {
        unlink(wlt_mem_state.path);
    }

    /* Zero out the key */
    memset(wlt_mem_state.key, 0, sizeof(wlt_mem_state.key));
    wlt_mem_state.key_len = 0;

    return WLT_AI_OK;
}

void
wlt_memory_add_message(wlt_ai_history_t *history, wlt_ai_role_t role,
                       const char *content)
{
    wlt_ai_message_t *msg;

    if (history == NULL || content == NULL)
        return;

    msg = &history->messages[history->head];
    memset(msg, 0, sizeof(*msg));

    msg->role = role;
    msg->timestamp = time(NULL);
    strncpy(msg->content, content, WLT_AI_MAX_MSG_LEN - 1);
    msg->content[WLT_AI_MAX_MSG_LEN - 1] = '\0';

    history->head = (history->head + 1) % WLT_AI_MAX_HISTORY;
    if (history->count < WLT_AI_MAX_HISTORY)
        history->count++;
}
