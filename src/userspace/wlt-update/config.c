/**
 * @file config.c
 * @brief Update agent configuration parsing
 * @module wlt-update/config
 * @purpose Loads /etc/wlt/update.conf, applies defaults, persists the
 *          auto-update flag, and exposes a tiny key=value line parser
 *          shared with the manifest reader. The format is a simple INI
 *          subset: blank lines and '#' comments are skipped, the single
 *          [update] section is optional.
 * @dependencies: update.h
 * @tested_by: tests/userspace/wlt_config_test.c
 * @spec: docs/specs/wlt-update.md
 * @adr: docs/adr/07-custom-init.md
 */

#include "update.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/* âââ Defaults âââââââââââââââââââââââââââââââââââââââââ
 * These mirror the spec's Tor + repository assumptions. The repo URL is
 * an .onion hidden service so checks leak no IP.
 */
void
wlt_config_defaults(wlt_UpdateConfig *out)
{
    if (out == NULL)
    {
        return;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->repo_url, "http://repo.wltios.onion/manifest",
            sizeof(out->repo_url) - 1);
    out->tor_socks_port = 9050;
    strncpy(out->current_version, "1.0.0", sizeof(out->current_version) - 1);
    out->auto_update = false;
    strncpy(out->pubkey_path, WLT_UPDATE_PUBKEY_PATH,
            sizeof(out->pubkey_path) - 1);
    strncpy(out->boot_config_path, WLT_UPDATE_BOOT_CONFIG_PATH,
            sizeof(out->boot_config_path) - 1);
}

/* âââ Internal: trim leading/trailing whitespace âââââââ
 * Operates in place and returns a pointer into @s. Used to strip the
 * "key = value" padding the config file allows.
 */
static char *
trim(char *s)
{
    while (*s != '\0' && isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == '\0')
    {
        return s;
    }
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
    {
        end--;
    }
    *end = '\0';
    return s;
}

/* ââ Public: bounded string copy (strlcpy semantics, always NUL-terminated)
 * Hand-rolled loop rather than strncpy so the build is clean under
 * -Wstringop-truncation while still satisfying the "no strcpy" rule. */
void
wlt_strlcpy(char *dst, const char *src, size_t n)
{
    if (dst == NULL || src == NULL || n == 0)
    {
        return;
    }
    size_t i = 0;
    while (i + 1 < n && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* âââ Internal: bounded key/value split ââââââââââââââââ
 * Splits @line at the first '='. Returns true on success with @key and
 * @val trimmed. Lines without '=' are rejected (caller decides). Both
 * outputs are always NUL-terminated.
 */
bool
wlt_config_parse_kv(const char *line,
                    char *key, size_t key_max,
                    char *val, size_t val_max)
{
    if (line == NULL || key == NULL || val == NULL ||
        key_max == 0 || val_max == 0)
    {
        return false;
    }
    key[0] = '\0';
    val[0] = '\0';

    /* Copy into a scratch buffer so trim() can mutate it. */
    char scratch[WLT_UPDATE_MAX_LINE];
    wlt_strlcpy(scratch, line, sizeof(scratch));

    char *eq = strchr(scratch, '=');
    if (eq == NULL)
    {
        return false;
    }
    *eq = '\0';

    char *k = trim(scratch);
    char *v = trim(eq + 1);
    if (k[0] == '\0')
    {
        return false;
    }

    wlt_strlcpy(key, k, key_max);
    wlt_strlcpy(val, v, val_max);
    return true;
}

/* âââ Public: load config ââââââââââââââââââââââââââââââ
 * Missing file is not fatal: we fall back to defaults so a fresh
 * install can still run `wltupdate check`.
 */
wlt_update_error_t
wlt_config_load(const char *path, wlt_UpdateConfig *out)
{
    if (path == NULL || out == NULL)
    {
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

    wlt_config_defaults(out);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        if (errno == ENOENT)
        {
            WLT_UPDATE_LOG_WRN("config %s missing â using defaults", path);
            return WLT_UPDATE_OK;
        }
        WLT_UPDATE_LOG_ERR("cannot open config %s: %s",
                           path, strerror(errno));
        return WLT_UPDATE_ERR_IO;
    }

    char line[WLT_UPDATE_MAX_LINE];
    char key[WLT_UPDATE_MAX_LINE];
    char val[WLT_UPDATE_MAX_LINE];
    int applied = 0;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *t = trim(line);
        if (*t == '\0' || *t == '#' || *t == '[')
        {
            /* Skip blanks, comments, and the [update] section header. */
            continue;
        }
        if (!wlt_config_parse_kv(t, key, sizeof(key), val, sizeof(val)))
        {
            continue;
        }

        if (strcmp(key, "repo_url") == 0)
        {
            wlt_strlcpy(out->repo_url, val, sizeof(out->repo_url));
            applied++;
        }
        else if (strcmp(key, "tor_socks_port") == 0)
        {
            long p = strtol(val, NULL, 10);
            if (p > 0 && p < 65536)
            {
                out->tor_socks_port = (uint16_t)p;
                applied++;
            }
        }
        else if (strcmp(key, "current_version") == 0)
        {
            wlt_strlcpy(out->current_version, val,
                        sizeof(out->current_version));
            applied++;
        }
        else if (strcmp(key, "auto_update") == 0)
        {
            out->auto_update =
                (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0 ||
                 strcmp(val, "1") == 0);
            applied++;
        }
        else if (strcmp(key, "pubkey_path") == 0)
        {
            wlt_strlcpy(out->pubkey_path, val, sizeof(out->pubkey_path));
            applied++;
        }
        else if (strcmp(key, "boot_config_path") == 0)
        {
            wlt_strlcpy(out->boot_config_path, val,
                        sizeof(out->boot_config_path));
            applied++;
        }
        /* Unknown keys are ignored: forward-compatible. */
    }

    fclose(fp);
    WLT_UPDATE_LOG_DBG("loaded %d config keys from %s", applied, path);
    return WLT_UPDATE_OK;
}

/* âââ Internal: write one key=value line âââââââââââââââ */
static void
write_kv(FILE *fp, const char *key, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(fp, "%s = ", key);
    vfprintf(fp, fmt, ap);
    fputc('\n', fp);
    va_end(ap);
}

/* âââ Public: persist config (atomic temp + rename) ââââ
 * Used by enable/disable-auto so the flag survives reboot. We write to
 * a sibling temp file and rename so a crash mid-write never leaves a
 * truncated config.
 */
wlt_update_error_t
wlt_config_save(const char *path, const wlt_UpdateConfig *cfg)
{
    if (path == NULL || cfg == NULL)
    {
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

    char tmp[WLT_UPDATE_MAX_PATH];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp))
    {
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

    /* O_EXCL|O_NOFOLLOW closes the symlink-swap race (F2); fsync file +
     * fsync dir gives crash durability (G3). */
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0 && errno == EEXIST) { (void)unlink(tmp); fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600); }
    if (fd < 0)
    {
        WLT_UPDATE_LOG_ERR("cannot write %s: %s", tmp, strerror(errno));
        return WLT_UPDATE_ERR_IO;
    }
    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) { close(fd); (void)unlink(tmp); return WLT_UPDATE_ERR_IO; }

    fprintf(fp, "# WLT-Update configuration â managed by wltupdate\n");
    fprintf(fp, "[update]\n");
    write_kv(fp, "repo_url", "%s", cfg->repo_url);
    write_kv(fp, "tor_socks_port", "%u", cfg->tor_socks_port);
    write_kv(fp, "current_version", "%s", cfg->current_version);
    write_kv(fp, "auto_update", "%s", cfg->auto_update ? "yes" : "no");
    write_kv(fp, "pubkey_path", "%s", cfg->pubkey_path);
    write_kv(fp, "boot_config_path", "%s", cfg->boot_config_path);

    if (fflush(fp) != 0)
    {
        WLT_UPDATE_LOG_ERR("flush failed on %s: %s", tmp, strerror(errno));
        (void)fclose(fp); (void)unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }
    if (fsync(fileno(fp)) < 0)
    {
        WLT_UPDATE_LOG_ERR("fsync failed on %s: %s", tmp, strerror(errno));
        (void)fclose(fp); (void)unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }
    if (fclose(fp) != 0)
    {
        WLT_UPDATE_LOG_ERR("close failed on %s: %s", tmp, strerror(errno));
        (void)unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }
    if (rename(tmp, path) != 0)
    {
        WLT_UPDATE_LOG_ERR("rename %s -> %s failed: %s",
                           tmp, path, strerror(errno));
        (void)unlink(tmp);
        return WLT_UPDATE_ERR_IO;
    }

    /* fsync the parent directory so the rename is durable across power loss. */
    char dirbuf[WLT_UPDATE_MAX_PATH];
    wlt_strlcpy(dirbuf, path, sizeof(dirbuf));
    char *slash = strrchr(dirbuf, '/');
    if (slash != NULL)
    {
        *slash = '\0';
        int dfd = open(dirbuf, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dfd >= 0) { (void)fsync(dfd); close(dfd); }
    }

    return WLT_UPDATE_OK;
}
