/**
 * @file manifest.c
 * @brief App manifest parser implementation for WLT-Permissions
 * @module wlt-permissions/manifest
 * @purpose Parses INI-style .manifest files describing app permissions
 * @dependencies: manifest.h
 * @tested_by: tests/userspace/permissions_manifest_test.c
 * @spec: docs/specs/wlt-permissions.md
 */

#include "manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

/* âââ Logging ââââââââââââââââââââââââââââââââââââââââââ
 * WLT-Permissions is a standalone library with no shared logger, so
 * diagnostics go to stderr. wlt-init redirects stderr to
 * /var/log/wlt/permissions.log in production. */
#define WLT_PM_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-permissions/manifest][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_PM_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-permissions/manifest][WRN] " fmt "\n", ##__VA_ARGS__)

/* âââ String Helpers âââââââââââââââââââââââââââââââââââ */

static char *
trim(char *str)
{
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != str)
        memmove(str, start, strlen(start) + 1);

    if (*str)
    {
        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
            *end-- = '\0';
    }
    return str;
}

/* Split "key = value", trimming both sides and stripping a trailing
 * inline (# ...) comment from the value. Returns 0 on success, -1 if
 * no '=' is present. */
static int
parse_kv(const char *line, char *key, int key_max, char *val, int val_max)
{
    const char *eq = strchr(line, '=');
    if (eq == NULL)
        return -1;

    int klen = (int)(eq - line);
    if (klen >= key_max)
        klen = key_max - 1;
    memcpy(key, line, (size_t)klen);
    key[klen] = '\0';
    trim(key);

    const char *vstart = eq + 1;
    const char *hash = strchr(vstart, '#');
    int vlen = (int)((hash != NULL ? hash : vstart + strlen(vstart)) - vstart);
    if (vlen >= val_max)
        vlen = val_max - 1;
    memcpy(val, vstart, (size_t)vlen);
    val[vlen] = '\0';
    trim(val);
    return 0;
}

static void
copy_bounded(char *dst, size_t dst_max, const char *src)
{
    if (dst_max == 0)
        return;
    size_t n = strlen(src);
    if (n >= dst_max)
        n = dst_max - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool
file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* âââ Id / Value Tables ââââââââââââââââââââââââââââââââ
 * Keep these in wlt_perms_id_t order so index == id. */

static const char *const g_id_names[WLT_PERM_COUNT] = {
    "network", "filesystem", "camera", "microphone", "location",
    "notifications", "clipboard", "contacts", "bluetooth", "usb",
    "background", "autostart",
};

wlt_perms_id_t
wlt_perms_id_from_str(const char *key)
{
    if (key == NULL)
        return WLT_PERM_COUNT;
    for (int i = 0; i < WLT_PERM_COUNT; i++)
    {
        if (strcmp(key, g_id_names[i]) == 0)
            return (wlt_perms_id_t)i;
    }
    return WLT_PERM_COUNT;
}

const char *
wlt_perms_id_str(wlt_perms_id_t id)
{
    if (id < 0 || id >= WLT_PERM_COUNT)
        return "?";
    return g_id_names[id];
}

wlt_perms_kind_t
wlt_perms_id_kind(wlt_perms_id_t id)
{
    if (id == WLT_PERM_NETWORK)
        return WLT_PERMS_KIND_NETWORK;
    if (id == WLT_PERM_FILESYSTEM)
        return WLT_PERMS_KIND_FILESYSTEM;
    if (id >= WLT_PERM_CAMERA && id <= WLT_PERM_USB)
        return WLT_PERMS_KIND_TRI;
    if (id == WLT_PERM_BACKGROUND || id == WLT_PERM_AUTOSTART)
        return WLT_PERMS_KIND_BOOL;
    return WLT_PERMS_KIND_TRI;  /* unreachable for valid ids */
}

const char *
wlt_perms_value_str(wlt_perms_id_t id, wlt_perms_value_t v)
{
    /* Network and filesystem spell their denied state "none" to match
     * the manifest grammar; everything else uses "deny". */
    switch (v)
    {
        case WLT_PERMS_DENY:
            if (id == WLT_PERM_NETWORK || id == WLT_PERM_FILESYSTEM)
                return "none";
            return "deny";
        case WLT_PERMS_ASK:   return "ask";
        case WLT_PERMS_ALLOW: return "allow";
        case WLT_PERMS_NET_TOR_ONLY: return "tor-only";
        case WLT_PERMS_NET_DIRECT:   return "direct";
        case WLT_PERMS_FS_HOME_RO:   return "home:ro";
        case WLT_PERMS_FS_HOME_RW:   return "home:rw";
        case WLT_PERMS_FS_FULL:      return "full";
        default:                     return "?";
    }
}

wlt_perms_manifest_error_t
wlt_perms_value_from_str(wlt_perms_kind_t kind, const char *str,
                         wlt_perms_value_t *out)
{
    if (str == NULL || out == NULL)
        return WLT_PERMS_MFST_ERR_INVALID_ARG;

    /* "deny" is universally legal and maps to DENY (== none for
     * network/fs). Accept it first so the persistence file can use a
     * single token regardless of kind. */
    if (strcmp(str, "deny") == 0)
    {
        *out = WLT_PERMS_DENY;
        return WLT_PERMS_MFST_OK;
    }

    switch (kind)
    {
        case WLT_PERMS_KIND_NETWORK:
            if (strcmp(str, "none") == 0)          *out = WLT_PERMS_DENY;
            else if (strcmp(str, "tor-only") == 0) *out = WLT_PERMS_NET_TOR_ONLY;
            else if (strcmp(str, "direct") == 0)   *out = WLT_PERMS_NET_DIRECT;
            else return WLT_PERMS_MFST_ERR_PARSE;
            break;
        case WLT_PERMS_KIND_FILESYSTEM:
            if (strcmp(str, "none") == 0)          *out = WLT_PERMS_DENY;
            else if (strcmp(str, "home:ro") == 0)  *out = WLT_PERMS_FS_HOME_RO;
            else if (strcmp(str, "home:rw") == 0)  *out = WLT_PERMS_FS_HOME_RW;
            else if (strcmp(str, "full") == 0)     *out = WLT_PERMS_FS_FULL;
            else return WLT_PERMS_MFST_ERR_PARSE;
            break;
        case WLT_PERMS_KIND_TRI:
            if (strcmp(str, "ask") == 0)           *out = WLT_PERMS_ASK;
            else if (strcmp(str, "allow") == 0)    *out = WLT_PERMS_ALLOW;
            else return WLT_PERMS_MFST_ERR_PARSE;
            break;
        case WLT_PERMS_KIND_BOOL:
            if (strcmp(str, "allow") == 0)         *out = WLT_PERMS_ALLOW;
            else return WLT_PERMS_MFST_ERR_PARSE;
            break;
        default:
            return WLT_PERMS_MFST_ERR_INVALID_ARG;
    }
    return WLT_PERMS_MFST_OK;
}

bool
wlt_perms_value_is_legal(wlt_perms_id_t id, wlt_perms_value_t v)
{
    switch (wlt_perms_id_kind(id))
    {
        case WLT_PERMS_KIND_NETWORK:
            return v == WLT_PERMS_DENY ||
                   v == WLT_PERMS_NET_TOR_ONLY ||
                   v == WLT_PERMS_NET_DIRECT;
        case WLT_PERMS_KIND_FILESYSTEM:
            return v == WLT_PERMS_DENY ||
                   v == WLT_PERMS_FS_HOME_RO ||
                   v == WLT_PERMS_FS_HOME_RW ||
                   v == WLT_PERMS_FS_FULL;
        case WLT_PERMS_KIND_TRI:
            return v == WLT_PERMS_DENY ||
                   v == WLT_PERMS_ASK ||
                   v == WLT_PERMS_ALLOW;
        case WLT_PERMS_KIND_BOOL:
            return v == WLT_PERMS_DENY || v == WLT_PERMS_ALLOW;
        default:
            return false;
    }
}

/* âââ Section Parsers ââââââââââââââââââââââââââââââââââ */

static wlt_perms_manifest_error_t
parse_app_section(const char *key, const char *val, wlt_PermsManifest *m)
{
    if (strcmp(key, "name") == 0)
        copy_bounded(m->name, sizeof(m->name), val);
    else if (strcmp(key, "exec") == 0)
        copy_bounded(m->exec, sizeof(m->exec), val);
    else if (strcmp(key, "icon") == 0)
        copy_bounded(m->icon, sizeof(m->icon), val);
    /* Unknown keys ignored â forward-compatibility */
    return WLT_PERMS_MFST_OK;
}

static wlt_perms_manifest_error_t
parse_permissions_section(const char *key, const char *val,
                          wlt_PermsManifest *m)
{
    wlt_perms_id_t id = wlt_perms_id_from_str(key);
    if (id == WLT_PERM_COUNT)
    {
        WLT_PM_LOG_WRN("Unknown permission key '%s' â ignored", key);
        return WLT_PERMS_MFST_OK;  /* forward-compatibility */
    }

    wlt_perms_value_t v;
    wlt_perms_manifest_error_t e =
        wlt_perms_value_from_str(wlt_perms_id_kind(id), val, &v);
    if (e != WLT_PERMS_MFST_OK)
    {
        WLT_PM_LOG_ERR("Invalid value '%s' for permission %s", val, key);
        return e;
    }

    m->values[id] = v;
    m->declared_mask |= (1u << id);
    return WLT_PERMS_MFST_OK;
}

static wlt_perms_manifest_error_t
dispatch_kv(const char *section, const char *key, const char *val,
            wlt_PermsManifest *m)
{
    if (strcmp(section, "app") == 0)
        return parse_app_section(key, val, m);
    if (strcmp(section, "permissions") == 0)
    {
        m->has_permissions_section = true;
        return parse_permissions_section(key, val, m);
    }
    /* Unknown sections ignored â forward-compatibility */
    return WLT_PERMS_MFST_OK;
}

/* âââ Top-Level Parser âââââââââââââââââââââââââââââââââ */

static char *
read_next_line(const char **ptr, char *buf, size_t buf_max)
{
    size_t i = 0;
    while (**ptr && **ptr != '\n' && i < buf_max - 1)
        buf[i++] = *(*ptr)++;
    buf[i] = '\0';
    if (**ptr == '\n')
        (*ptr)++;
    return trim(buf);
}

wlt_perms_manifest_error_t
wlt_perms_manifest_parse_text(const char *text, wlt_PermsManifest *m)
{
    if (text == NULL || m == NULL)
        return WLT_PERMS_MFST_ERR_INVALID_ARG;

    char section[32] = "";
    const char *ptr = text;

    while (*ptr)
    {
        char line[WLT_PERMS_MAX_LINE];
        char *trimmed = read_next_line(&ptr, line, sizeof(line));
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        if (*trimmed == '[')
        {
            char *end = strchr(trimmed, ']');
            if (end == NULL)
            {
                WLT_PM_LOG_ERR("Unterminated section header: %s", trimmed);
                return WLT_PERMS_MFST_ERR_PARSE;
            }
            *end = '\0';
            copy_bounded(section, sizeof(section), trimmed + 1);
            continue;
        }

        char key[128], val[WLT_PERMS_MAX_LINE];
        if (parse_kv(trimmed, key, sizeof(key), val, sizeof(val)) != 0)
        {
            WLT_PM_LOG_WRN("Skipping malformed line: %s", trimmed);
            continue;
        }

        wlt_perms_manifest_error_t e = dispatch_kv(section, key, val, m);
        if (e != WLT_PERMS_MFST_OK)
            return e;
    }
    return WLT_PERMS_MFST_OK;
}

/* âââ Validation âââââââââââââââââââââââââââââââââââââââ */

wlt_perms_manifest_error_t
wlt_perms_manifest_validate(const wlt_PermsManifest *m)
{
    if (m == NULL)
        return WLT_PERMS_MFST_ERR_INVALID_ARG;
    if (m->name[0] == '\0')
    {
        WLT_PM_LOG_ERR("Manifest missing [app] name");
        return WLT_PERMS_MFST_ERR_PARSE;
    }
    if (m->exec[0] == '\0')
    {
        WLT_PM_LOG_ERR("Manifest %s missing exec", m->name);
        return WLT_PERMS_MFST_ERR_PARSE;
    }
    if (!file_exists(m->exec))
        WLT_PM_LOG_WRN("Manifest %s: exec %s not present", m->name, m->exec);

    /* Spec Â§Error: a manifest with no [permissions] section gets all
     * permissions denied. We don't fail validation (an app may
     * legitimately declare none) â the manager logs the warning. */
    if (!m->has_permissions_section)
        WLT_PM_LOG_WRN("Manifest %s has no [permissions] section", m->name);

    for (int i = 0; i < WLT_PERM_COUNT; i++)
    {
        if ((m->declared_mask & (1u << i)) == 0)
            continue;
        if (!wlt_perms_value_is_legal((wlt_perms_id_t)i, m->values[i]))
        {
            WLT_PM_LOG_ERR("Manifest %s: illegal value for %s",
                           m->name, wlt_perms_id_str((wlt_perms_id_t)i));
            return WLT_PERMS_MFST_ERR_PARSE;
        }
    }
    return WLT_PERMS_MFST_OK;
}

/* âââ Public API âââââââââââââââââââââââââââââââââââââââ */

void
wlt_perms_manifest_default(wlt_PermsManifest *m)
{
    if (m == NULL)
        return;
    memset(m, 0, sizeof(*m));
    for (int i = 0; i < WLT_PERM_COUNT; i++)
        m->values[i] = WLT_PERMS_DENY;
    m->declared_mask = 0;
    m->has_permissions_section = false;
}

wlt_perms_manifest_error_t
wlt_perms_manifest_load(const char *path, wlt_PermsManifest *m)
{
    if (path == NULL || m == NULL)
        return WLT_PERMS_MFST_ERR_INVALID_ARG;
    if (strlen(path) >= WLT_PERMS_MAX_PATH)
        return WLT_PERMS_MFST_ERR_TOO_LARGE;

    wlt_perms_manifest_default(m);

    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        WLT_PM_LOG_ERR("Cannot open %s: %s", path, strerror(errno));
        return (errno == ENOENT) ? WLT_PERMS_MFST_ERR_NOT_FOUND
                                 : WLT_PERMS_MFST_ERR_IO;
    }

    /* Manifests are small (<4 KiB); slurp in one read. */
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    int saved_errno = ferror(f) ? errno : 0;
    fclose(f);
    if (saved_errno != 0)
    {
        WLT_PM_LOG_ERR("Read error on %s: %s", path, strerror(saved_errno));
        return WLT_PERMS_MFST_ERR_IO;
    }

    wlt_perms_manifest_error_t e = wlt_perms_manifest_parse_text(buf, m);
    if (e != WLT_PERMS_MFST_OK)
        return e;
    return wlt_perms_manifest_validate(m);
}

const char *
wlt_perms_manifest_error_str(wlt_perms_manifest_error_t err)
{
    switch (err)
    {
        case WLT_PERMS_MFST_OK:            return "ok";
        case WLT_PERMS_MFST_ERR_INVALID_ARG: return "invalid argument";
        case WLT_PERMS_MFST_ERR_NOT_FOUND:   return "not found";
        case WLT_PERMS_MFST_ERR_IO:          return "i/o error";
        case WLT_PERMS_MFST_ERR_PARSE:       return "parse error";
        case WLT_PERMS_MFST_ERR_NOMEM:       return "out of memory";
        case WLT_PERMS_MFST_ERR_TOO_LARGE:   return "too large";
        default:                             return "unknown error";
    }
}
