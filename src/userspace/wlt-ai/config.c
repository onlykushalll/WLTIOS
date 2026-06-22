/*
 * config.c - WLT-AI Configuration Parsing
 *
 * Parses the INI-format configuration file for the WLT-AI assistant.
 * The config controls: operating mode (local/cloud/hybrid), hotword,
 * voice output, memory persistence, model names, and cloud credentials.
 *
 * Config file format (wlt-ai.conf):
 *
 *   [wlt-ai]
 *   enabled = yes
 *   mode = local            # local | cloud | hybrid
 *   hotword = "hey wltios"
 *   voice_output = yes
 *   memory = amnesic        # amnesic | persistent
 *
 *   [models]
 *   vlm = llama-vision-7b
 *   llm = llama-3-8b
 *   stt = whisper-small
 *   tts = coqui-tts
 *
 *   [endpoints]
 *   llm = http://localhost:8080
 *   vlm = http://localhost:8081
 *   stt = http://localhost:8082
 *   tts = http://localhost:8083
 *
 *   [cloud]
 *   provider = none         # none | gemini | claude | glm
 *   api_key =               # from Key Vault
 *   endpoint =              # custom endpoint (optional)
 *
 * Copyright (c) WLTIOS Project. All rights reserved.
 */

#include "ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define WLT_CFG_MAX_LINE    512
#define WLT_CFG_MAX_SECTIONS 8
#define WLT_CFG_MAX_KEYS    32

/* ------------------------------------------------------------------ */
/* Internal types                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char    section[64];
    char    key[64];
    char    value[256];
} wlt_cfg_entry_t;

typedef struct {
    wlt_cfg_entry_t  entries[WLT_CFG_MAX_KEYS];
    size_t           count;
} wlt_cfg_table_t;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void
wlt_cfg_trim(char *s)
{
    char *start, *end;

    start = s;
    while (*start && isspace((unsigned char)*start))
        start++;

    if (*start == '\0') {
        *s = '\0';
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';

    if (start != s)
        memmove(s, start, strlen(start) + 1);
}

static void
wlt_cfg_unquote(char *s)
{
    size_t len;

    wlt_cfg_trim(s);
    len = strlen(s);

    if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static bool
wlt_cfg_parse_bool(const char *val, bool default_val)
{
    if (val == NULL)
        return default_val;

    if (strcasecmp(val, "yes") == 0 ||
        strcasecmp(val, "true") == 0 ||
        strcasecmp(val, "1") == 0 ||
        strcasecmp(val, "on") == 0)
        return true;

    if (strcasecmp(val, "no") == 0 ||
        strcasecmp(val, "false") == 0 ||
        strcasecmp(val, "0") == 0 ||
        strcasecmp(val, "off") == 0)
        return false;

    return default_val;
}

/* ------------------------------------------------------------------ */
/* INI parser                                                         */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t
wlt_cfg_parse_file(FILE *f, wlt_cfg_table_t *table)
{
    char line[WLT_CFG_MAX_LINE];
    char current_section[64] = "";
    size_t line_num = 0;

    table->count = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p;
        char *eq;
        char *key;
        char *val;
        size_t len;

        line_num++;
        len = strlen(line);

        /* Strip trailing newline */
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r')
            line[--len] = '\0';

        wlt_cfg_trim(line);

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        /* Section header */
        if (line[0] == '[') {
            p = strchr(line, ']');
            if (p == NULL) {
                fprintf(stderr, "wlt-ai: config line %zu: "
                        "missing ']'\n", line_num);
                continue;
            }
            *p = '\0';
            wlt_cfg_trim(line + 1);
            strncpy(current_section, line + 1, sizeof(current_section) - 1);
            current_section[sizeof(current_section) - 1] = '\0';
            continue;
        }

        /* Key = Value */
        eq = strchr(line, '=');
        if (eq == NULL) {
            fprintf(stderr, "wlt-ai: config line %zu: "
                    "missing '='\n", line_num);
            continue;
        }

        *eq = '\0';
        key = line;
        val = eq + 1;

        wlt_cfg_trim(key);
        wlt_cfg_unquote(val);

        /* Store in table */
        if (table->count >= WLT_CFG_MAX_KEYS) {
            fprintf(stderr, "wlt-ai: config too many keys (max %d)\n",
                    WLT_CFG_MAX_KEYS);
            break;
        }

        {
            wlt_cfg_entry_t *entry = &table->entries[table->count++];
            strncpy(entry->section, current_section,
                    sizeof(entry->section) - 1);
            entry->section[sizeof(entry->section) - 1] = '\0';
            strncpy(entry->key, key, sizeof(entry->key) - 1);
            entry->key[sizeof(entry->key) - 1] = '\0';
            strncpy(entry->value, val, sizeof(entry->value) - 1);
            entry->value[sizeof(entry->value) - 1] = '\0';
        }
    }

    return WLT_AI_OK;
}

static const char *
wlt_cfg_lookup(const wlt_cfg_table_t *table,
               const char *section, const char *key)
{
    size_t i;

    for (i = 0; i < table->count; i++) {
        if (strcasecmp(table->entries[i].section, section) == 0 &&
            strcasecmp(table->entries[i].key, key) == 0)
            return table->entries[i].value;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Config application                                                 */
/* ------------------------------------------------------------------ */

static void
wlt_cfg_set_defaults(wlt_ai_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->enabled = true;
    cfg->mode = WLT_AI_MODE_LOCAL;
    strncpy(cfg->hotword, "hey wltios", sizeof(cfg->hotword) - 1);
    cfg->voice_output = true;
    cfg->memory_mode = WLT_AI_MEMORY_AMNESIC;

    strncpy(cfg->vlm_model, "llama-vision-7b", sizeof(cfg->vlm_model) - 1);
    strncpy(cfg->llm_model, "llama-3-8b", sizeof(cfg->llm_model) - 1);
    strncpy(cfg->stt_model, "whisper-small", sizeof(cfg->stt_model) - 1);
    strncpy(cfg->tts_model, "coqui-tts", sizeof(cfg->tts_model) - 1);

    strncpy(cfg->local_llm_endpoint, "http://localhost:8080",
            sizeof(cfg->local_llm_endpoint) - 1);
    strncpy(cfg->local_vlm_endpoint, "http://localhost:8081",
            sizeof(cfg->local_vlm_endpoint) - 1);
    strncpy(cfg->local_stt_endpoint, "http://localhost:8082",
            sizeof(cfg->local_stt_endpoint) - 1);
    strncpy(cfg->local_tts_endpoint, "http://localhost:8083",
            sizeof(cfg->local_tts_endpoint) - 1);

    cfg->cloud_provider = WLT_AI_CLOUD_NONE;
}

static void
wlt_cfg_derive_key(wlt_ai_config_t *cfg)
{
    /*
     * Derive an encryption key for memory from /dev/urandom.
     * In production, this should come from the WLTIOS Key Vault
     * (user passphrase â Argon2 â 256-bit key).
     *
     * For amnesic mode, we leave the key as all-zeros, which
     * signals to the memory subsystem not to persist anything.
     */
    if (cfg->memory_mode != WLT_AI_MEMORY_PERSISTENT)
        return;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* Fallback: use a deterministic but unique-per-process seed.
         * This is weak but better than nothing if /dev/urandom is
         * unavailable. */
        srand((unsigned)(getpid() ^ (int)time(NULL)));
        for (size_t i = 0; i < WLT_AI_ENCRYPT_KEY_LEN; i++)
            cfg->encrypt_key[i] = (uint8_t)(rand() & 0xFF);
        return;
    }

    ssize_t n = read(fd, cfg->encrypt_key, WLT_AI_ENCRYPT_KEY_LEN);
    close(fd);

    if (n != WLT_AI_ENCRYPT_KEY_LEN) {
        /* Partial read â fill the rest deterministically */
        for (size_t i = (size_t)n; i < WLT_AI_ENCRYPT_KEY_LEN; i++)
            cfg->encrypt_key[i] = (uint8_t)(rand() & 0xFF);
    }
}

static void
wlt_cfg_apply(wlt_ai_config_t *cfg, const wlt_cfg_table_t *table)
{
    const char *val;

    /* [wlt-ai] section */
    val = wlt_cfg_lookup(table, "wlt-ai", "enabled");
    if (val) cfg->enabled = wlt_cfg_parse_bool(val, cfg->enabled);

    val = wlt_cfg_lookup(table, "wlt-ai", "mode");
    if (val) {
        if (strcasecmp(val, "local") == 0)
            cfg->mode = WLT_AI_MODE_LOCAL;
        else if (strcasecmp(val, "cloud") == 0)
            cfg->mode = WLT_AI_MODE_CLOUD;
        else if (strcasecmp(val, "hybrid") == 0)
            cfg->mode = WLT_AI_MODE_HYBRID;
    }

    val = wlt_cfg_lookup(table, "wlt-ai", "hotword");
    if (val) {
        strncpy(cfg->hotword, val, sizeof(cfg->hotword) - 1);
        cfg->hotword[sizeof(cfg->hotword) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "wlt-ai", "voice_output");
    if (val) cfg->voice_output = wlt_cfg_parse_bool(val, cfg->voice_output);

    val = wlt_cfg_lookup(table, "wlt-ai", "memory");
    if (val) {
        if (strcasecmp(val, "amnesic") == 0)
            cfg->memory_mode = WLT_AI_MEMORY_AMNESIC;
        else if (strcasecmp(val, "persistent") == 0)
            cfg->memory_mode = WLT_AI_MEMORY_PERSISTENT;
    }

    /* [models] section */
    val = wlt_cfg_lookup(table, "models", "vlm");
    if (val) {
        strncpy(cfg->vlm_model, val, sizeof(cfg->vlm_model) - 1);
        cfg->vlm_model[sizeof(cfg->vlm_model) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "models", "llm");
    if (val) {
        strncpy(cfg->llm_model, val, sizeof(cfg->llm_model) - 1);
        cfg->llm_model[sizeof(cfg->llm_model) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "models", "stt");
    if (val) {
        strncpy(cfg->stt_model, val, sizeof(cfg->stt_model) - 1);
        cfg->stt_model[sizeof(cfg->stt_model) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "models", "tts");
    if (val) {
        strncpy(cfg->tts_model, val, sizeof(cfg->tts_model) - 1);
        cfg->tts_model[sizeof(cfg->tts_model) - 1] = '\0';
    }

    /* [endpoints] section */
    val = wlt_cfg_lookup(table, "endpoints", "llm");
    if (val) {
        strncpy(cfg->local_llm_endpoint, val,
                sizeof(cfg->local_llm_endpoint) - 1);
        cfg->local_llm_endpoint[sizeof(cfg->local_llm_endpoint) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "endpoints", "vlm");
    if (val) {
        strncpy(cfg->local_vlm_endpoint, val,
                sizeof(cfg->local_vlm_endpoint) - 1);
        cfg->local_vlm_endpoint[sizeof(cfg->local_vlm_endpoint) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "endpoints", "stt");
    if (val) {
        strncpy(cfg->local_stt_endpoint, val,
                sizeof(cfg->local_stt_endpoint) - 1);
        cfg->local_stt_endpoint[sizeof(cfg->local_stt_endpoint) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "endpoints", "tts");
    if (val) {
        strncpy(cfg->local_tts_endpoint, val,
                sizeof(cfg->local_tts_endpoint) - 1);
        cfg->local_tts_endpoint[sizeof(cfg->local_tts_endpoint) - 1] = '\0';
    }

    /* [cloud] section */
    val = wlt_cfg_lookup(table, "cloud", "provider");
    if (val) {
        if (strcasecmp(val, "none") == 0)
            cfg->cloud_provider = WLT_AI_CLOUD_NONE;
        else if (strcasecmp(val, "gemini") == 0)
            cfg->cloud_provider = WLT_AI_CLOUD_GEMINI;
        else if (strcasecmp(val, "claude") == 0)
            cfg->cloud_provider = WLT_AI_CLOUD_CLAUDE;
        else if (strcasecmp(val, "glm") == 0)
            cfg->cloud_provider = WLT_AI_CLOUD_GLM;
    }

    val = wlt_cfg_lookup(table, "cloud", "api_key");
    if (val) {
        strncpy(cfg->cloud_api_key, val,
                sizeof(cfg->cloud_api_key) - 1);
        cfg->cloud_api_key[sizeof(cfg->cloud_api_key) - 1] = '\0';
    }

    val = wlt_cfg_lookup(table, "cloud", "endpoint");
    if (val) {
        strncpy(cfg->cloud_endpoint, val,
                sizeof(cfg->cloud_endpoint) - 1);
        cfg->cloud_endpoint[sizeof(cfg->cloud_endpoint) - 1] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

wlt_ai_err_t
wlt_config_load(const char *path, wlt_ai_config_t *cfg)
{
    FILE *f;
    wlt_cfg_table_t table;
    wlt_ai_err_t err;

    if (cfg == NULL)
        return WLT_AI_ERR_INVALID;

    /* Set defaults first */
    wlt_cfg_set_defaults(cfg);

    if (path == NULL)
        path = WLT_AI_CONFIG_PATH;

    f = fopen(path, "r");
    if (f == NULL) {
        /* Config file doesn't exist â use defaults */
        if (errno != ENOENT) {
            fprintf(stderr, "wlt-ai: cannot open config %s: %s\n",
                    path, strerror(errno));
            return WLT_AI_ERR_CONFIG;
        }
        /* Derive encryption key with defaults */
        wlt_cfg_derive_key(cfg);
        return WLT_AI_OK;
    }

    err = wlt_cfg_parse_file(f, &table);
    fclose(f);

    if (err != WLT_AI_OK)
        return err;

    /* Apply parsed values over defaults */
    wlt_cfg_apply(cfg, &table);

    /* Derive encryption key based on memory mode */
    wlt_cfg_derive_key(cfg);

    /* Validate: if cloud mode but no provider/key, fall back to local */
    if ((cfg->mode == WLT_AI_MODE_CLOUD ||
         cfg->mode == WLT_AI_MODE_HYBRID) &&
        cfg->cloud_provider == WLT_AI_CLOUD_NONE) {
        fprintf(stderr, "wlt-ai: cloud mode requested but no provider "
                "configured â falling back to local\n");
        cfg->mode = WLT_AI_MODE_LOCAL;
    }

    return WLT_AI_OK;
}

void
wlt_config_free(wlt_ai_config_t *cfg)
{
    if (cfg == NULL)
        return;

    /* Securely wipe sensitive fields */
    memset(cfg->cloud_api_key, 0, sizeof(cfg->cloud_api_key));
    memset(cfg->encrypt_key, 0, sizeof(cfg->encrypt_key));
    memset(cfg, 0, sizeof(*cfg));
}

/* ------------------------------------------------------------------ */
/* Utility functions                                                  */
/* ------------------------------------------------------------------ */

const char *
wlt_ai_strerror(wlt_ai_err_t err)
{
    switch (err) {
    case WLT_AI_OK:          return "success";
    case WLT_AI_ERR_INVALID: return "invalid argument";
    case WLT_AI_ERR_CONFIG:  return "configuration error";
    case WLT_AI_ERR_NETWORK: return "network error";
    case WLT_AI_ERR_MEMORY:  return "memory error";
    case WLT_AI_ERR_MODEL:   return "model error";
    case WLT_AI_ERR_TOOL:    return "tool error";
    case WLT_AI_ERR_ENCRYPT: return "encryption error";
    case WLT_AI_ERR_IO:      return "I/O error";
    case WLT_AI_ERR_NOMEM:   return "out of memory";
    default:                 return "unknown error";
    }
}

const char *
wlt_tool_type_str(wlt_tool_type_t type)
{
    switch (type) {
    case WLT_TOOL_NONE:           return "none";
    case WLT_TOOL_OPEN_APP:       return "open_app";
    case WLT_TOOL_CHANGE_SETTING: return "change_setting";
    case WLT_TOOL_RUN_COMMAND:    return "run_command";
    case WLT_TOOL_FILE_SEARCH:    return "file_search";
    case WLT_TOOL_SCREEN_ANALYZE: return "screen_analyze";
    default:                      return "unknown";
    }
}
