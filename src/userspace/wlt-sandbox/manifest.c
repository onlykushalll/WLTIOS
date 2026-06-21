/**
 * @file manifest.c
 * @brief App manifest parser implementation
 * @module wlt-sandbox/manifest
 * @purpose Parses INI-style .manifest files describing app permissions
 * @dependencies: manifest.h
 * @tested_by: tests/userspace/manifest_test.c
 * @spec: docs/specs/wlt-sandbox.md
 */

#include "manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

/* ─── Logging ──────────────────────────────────────────
 * WLT-Sandbox ships as a standalone daemon (no shared log.h), so we
 * emit diagnostics to stderr. The init system redirects stderr to
 * /var/log/wlt/sandbox.log in production. */
#define WLT_MFST_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/manifest][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_MFST_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/manifest][WRN] " fmt "\n", ##__VA_ARGS__)
#define WLT_MFST_LOG_DBG(fmt, ...) \
    fprintf(stderr, "[wlt-sandbox/manifest][DBG] " fmt "\n", ##__VA_ARGS__)

/* ─── String Helpers ─────────────────────────────────── */

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

/* Split "key = value" into two trimmed buffers. Returns 0 on success,
 * -1 if no '=' is present. Strips trailing inline comments (# ...) so
 * manifest authors can annotate individual lines. */
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

    /* Copy value up to a comment marker or end of line */
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

/* Bounded copy: guarantees NUL-termination. Avoids the strncpy
 * "no NUL if src longer than n" footgun. */
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

/* ─── Section Parsers ────────────────────────────────── */

static wlt_manifest_error_t
parse_app_section(const char *key, const char *val, wlt_SandboxManifest *m)
{
    if (strcmp(key, "name") == 0)
        copy_bounded(m->name, sizeof(m->name), val);
    else if (strcmp(key, "exec") == 0)
        copy_bounded(m->exec, sizeof(m->exec), val);
    else if (strcmp(key, "icon") == 0)
        copy_bounded(m->icon, sizeof(m->icon), val);
    else if (strcmp(key, "args") == 0)
        copy_bounded(m->args, sizeof(m->args), val);
    else if (strcmp(key, "user") == 0)
        copy_bounded(m->user, sizeof(m->user), val);
    /* Unknown keys are ignored — forward-compatibility */
    return WLT_MANIFEST_OK;
}

static wlt_manifest_error_t
parse_permissions_section(const char *key, const char *val,
                          wlt_SandboxManifest *m)
{
    if (strcmp(key, "network") == 0)
    {
        if (strcmp(val, "tor-only") == 0)      m->network = WLT_NET_TOR_ONLY;
        else if (strcmp(val, "direct") == 0)   m->network = WLT_NET_DIRECT;
        else if (strcmp(val, "none") == 0)     m->network = WLT_NET_NONE;
        else
        {
            WLT_MFST_LOG_ERR("Invalid network mode: %s", val);
            return WLT_MANIFEST_ERR_PARSE;
        }
    }
    else if (strcmp(key, "filesystem") == 0)
    {
        if (strcmp(val, "none") == 0)          m->filesystem = WLT_FS_NONE;
        else if (strcmp(val, "home:ro") == 0)  m->filesystem = WLT_FS_HOME_RO;
        else if (strcmp(val, "home:rw") == 0)  m->filesystem = WLT_FS_HOME_RW;
        else if (strcmp(val, "full") == 0)     m->filesystem = WLT_FS_FULL;
        else
        {
            WLT_MFST_LOG_ERR("Invalid filesystem mode: %s", val);
            return WLT_MANIFEST_ERR_PARSE;
        }
    }
    else
    {
        /* Tri-state perms: allow | deny | ask */
        wlt_manifest_perm_t p;
        if (strcmp(val, "allow") == 0)         p = WLT_PERM_ALLOW;
        else if (strcmp(val, "deny") == 0)     p = WLT_PERM_DENY;
        else if (strcmp(val, "ask") == 0)      p = WLT_PERM_ASK;
        else
        {
            WLT_MFST_LOG_ERR("Invalid permission value for %s: %s", key, val);
            return WLT_MANIFEST_ERR_PARSE;
        }

        if (strcmp(key, "camera") == 0)              m->camera = p;
        else if (strcmp(key, "microphone") == 0)     m->microphone = p;
        else if (strcmp(key, "location") == 0)       m->location = p;
        else if (strcmp(key, "notifications") == 0)  m->notifications = p;
        else if (strcmp(key, "clipboard") == 0)      m->clipboard = p;
        /* Unknown perms ignored */
    }
    return WLT_MANIFEST_OK;
}

static wlt_manifest_error_t
parse_resources_section(const char *key, const char *val,
                        wlt_SandboxManifest *m)
{
    /* Reject anything that isn't a small non-negative integer.
     * strtoul returns 0 for garbage — we accept that as "default". */
    char *end = NULL;
    unsigned long n = strtoul(val, &end, 10);
    if (end == val || (end != NULL && *end != '\0'))
    {
        WLT_MFST_LOG_ERR("Resource %s: not a number: %s", key, val);
        return WLT_MANIFEST_ERR_PARSE;
    }
    if (n > 0xFFFFFFFFUL)
    {
        WLT_MFST_LOG_ERR("Resource %s: value too large: %lu", key, n);
        return WLT_MANIFEST_ERR_TOO_LARGE;
    }

    if (strcmp(key, "memory_mb") == 0)         m->resources.memory_mb = (uint32_t)n;
    else if (strcmp(key, "cpu_percent") == 0)  m->resources.cpu_percent = (uint32_t)n;
    else if (strcmp(key, "disk_mb") == 0)      m->resources.disk_mb = (uint32_t)n;
    else if (strcmp(key, "pids") == 0)         m->resources.pids = (uint32_t)n;
    else if (strcmp(key, "network_kbps") == 0) m->resources.network_kbps = (uint32_t)n;
    return WLT_MANIFEST_OK;
}

static wlt_manifest_error_t
parse_seccomp_section(const char *key, const char *val, wlt_SandboxManifest *m)
{
    if (strcmp(key, "allowed") != 0)
        return WLT_MANIFEST_OK;  /* ignore unknown seccomp keys */

    uint32_t mask = 0;
    wlt_manifest_error_t e = wlt_manifest_parse_seccomp_groups(val, &mask);
    if (e != WLT_MANIFEST_OK)
        return e;
    m->seccomp_groups = mask;
    return WLT_MANIFEST_OK;
}

/* ─── Top-Level Parser ───────────────────────────────── */

/* Dispatch a single key/value pair to the appropriate section parser.
 * Returns the parser's error code (WLT_MANIFEST_OK on success or
 * "section not recognised" — the latter is forward-compatible). */
static wlt_manifest_error_t
dispatch_kv(const char *section, const char *key, const char *val,
            wlt_SandboxManifest *m)
{
    if (strcmp(section, "app") == 0)
        return parse_app_section(key, val, m);
    if (strcmp(section, "permissions") == 0)
        return parse_permissions_section(key, val, m);
    if (strcmp(section, "resources") == 0)
        return parse_resources_section(key, val, m);
    if (strcmp(section, "seccomp") == 0)
        return parse_seccomp_section(key, val, m);
    /* Unknown sections ignored — forward-compatibility */
    return WLT_MANIFEST_OK;
}

/* Read one line from *ptr into buf, advancing *ptr past the newline.
 * Returns the trimmed line (may be empty). */
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

static wlt_manifest_error_t
parse_manifest_text(const char *text, wlt_SandboxManifest *m)
{
    char section[32] = "";
    const char *ptr = text;

    while (*ptr)
    {
        char line[WLT_MANIFEST_MAX_LINE];
        char *trimmed = read_next_line(&ptr, line, sizeof(line));
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        /* Section header: [name] */
        if (*trimmed == '[')
        {
            char *end = strchr(trimmed, ']');
            if (end == NULL)
            {
                WLT_MFST_LOG_ERR("Unterminated section header: %s", trimmed);
                return WLT_MANIFEST_ERR_PARSE;
            }
            *end = '\0';
            copy_bounded(section, sizeof(section), trimmed + 1);
            continue;
        }

        char key[128], val[WLT_MANIFEST_MAX_LINE];
        if (parse_kv(trimmed, key, sizeof(key), val, sizeof(val)) != 0)
        {
            WLT_MFST_LOG_WRN("Skipping malformed line: %s", trimmed);
            continue;
        }

        wlt_manifest_error_t e = dispatch_kv(section, key, val, m);
        if (e != WLT_MANIFEST_OK)
            return e;
    }
    return WLT_MANIFEST_OK;
}

/* ─── Validation ─────────────────────────────────────── */

static bool
in_range(uint32_t v, uint32_t lo, uint32_t hi)
{
    return v >= lo && v <= hi;
}

static bool
file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Verify each resource limit is in its permitted range. Returns
 * WLT_MANIFEST_OK if all four are valid, otherwise the parse error.
 * Defaults are applied for zero-valued fields before the range check. */
static wlt_manifest_error_t
validate_resources(const wlt_SandboxManifest *m)
{
    uint32_t mem  = m->resources.memory_mb   ? m->resources.memory_mb
                                              : WLT_MANIFEST_DEFAULT_MEM_MB;
    uint32_t cpu  = m->resources.cpu_percent ? m->resources.cpu_percent
                                              : WLT_MANIFEST_DEFAULT_CPU_PCT;
    uint32_t disk = m->resources.disk_mb     ? m->resources.disk_mb
                                              : WLT_MANIFEST_DEFAULT_DISK_MB;
    uint32_t pids = m->resources.pids        ? m->resources.pids
                                              : WLT_MANIFEST_DEFAULT_PIDS;

    if (!in_range(mem, 1, WLT_MANIFEST_MAX_MEM_MB))
    {
        WLT_MFST_LOG_ERR("Manifest %s: memory_mb %u out of range", m->name, mem);
        return WLT_MANIFEST_ERR_PARSE;
    }
    if (!in_range(cpu, 1, WLT_MANIFEST_MAX_CPU_PCT))
    {
        WLT_MFST_LOG_ERR("Manifest %s: cpu_percent %u out of range", m->name, cpu);
        return WLT_MANIFEST_ERR_PARSE;
    }
    if (!in_range(disk, 1, WLT_MANIFEST_MAX_DISK_MB))
    {
        WLT_MFST_LOG_ERR("Manifest %s: disk_mb %u out of range", m->name, disk);
        return WLT_MANIFEST_ERR_PARSE;
    }
    if (!in_range(pids, 1, WLT_MANIFEST_MAX_PIDS))
    {
        WLT_MFST_LOG_ERR("Manifest %s: pids %u out of range", m->name, pids);
        return WLT_MANIFEST_ERR_PARSE;
    }
    return WLT_MANIFEST_OK;
}

wlt_manifest_error_t
wlt_manifest_validate(const wlt_SandboxManifest *m)
{
    if (m == NULL)
        return WLT_MANIFEST_ERR_INVALID_ARG;
    if (m->name[0] == '\0')
    {
        WLT_MFST_LOG_ERR("Manifest missing [app] name");
        return WLT_MANIFEST_ERR_PARSE;
    }
    if (m->exec[0] == '\0')
    {
        WLT_MFST_LOG_ERR("Manifest %s missing exec", m->name);
        return WLT_MANIFEST_ERR_PARSE;
    }
    /* Don't fail validation on missing binary — tests run without
     * the app installed. The launcher reports the runtime error. */
    if (!file_exists(m->exec))
        WLT_MFST_LOG_WRN("Manifest %s: exec %s not present", m->name, m->exec);

    wlt_manifest_error_t e = validate_resources(m);
    if (e != WLT_MANIFEST_OK)
        return e;

    /* BASIC seccomp group is mandatory — without it the app cannot
     * even call exit() and would be killed by SIGSYS on the first
     * syscall. */
    if ((m->seccomp_groups & (1u << WLT_SECCOMP_GRP_BASIC)) == 0)
    {
        WLT_MFST_LOG_ERR("Manifest %s: seccomp must allow 'basic' group",
                         m->name);
        return WLT_MANIFEST_ERR_PARSE;
    }
    return WLT_MANIFEST_OK;
}

/* ─── Public API ─────────────────────────────────────── */

void
wlt_manifest_default(wlt_SandboxManifest *m)
{
    if (m == NULL)
        return;
    memset(m, 0, sizeof(*m));

    /* Default-deny: every permission starts locked down */
    m->network       = WLT_NET_NONE;
    m->filesystem    = WLT_FS_NONE;
    m->camera        = WLT_PERM_DENY;
    m->microphone    = WLT_PERM_DENY;
    m->location      = WLT_PERM_DENY;
    m->notifications = WLT_PERM_DENY;
    m->clipboard     = WLT_PERM_DENY;

    /* Zero resources means "use defaults" — applied at validate time */
    m->seccomp_groups = 0;
    copy_bounded(m->user, sizeof(m->user), "wlt");
}

wlt_manifest_error_t
wlt_manifest_load(const char *path, wlt_SandboxManifest *m)
{
    if (path == NULL || m == NULL)
        return WLT_MANIFEST_ERR_INVALID_ARG;
    if (strlen(path) >= WLT_MANIFEST_MAX_PATH)
        return WLT_MANIFEST_ERR_TOO_LARGE;

    wlt_manifest_default(m);

    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        WLT_MFST_LOG_ERR("Cannot open %s: %s", path, strerror(errno));
        return (errno == ENOENT) ? WLT_MANIFEST_ERR_NOT_FOUND
                                 : WLT_MANIFEST_ERR_IO;
    }

    /* Read whole file in one slurp. Manifests are small (<4 KiB). */
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    int saved_errno = ferror(f) ? errno : 0;
    fclose(f);
    if (saved_errno != 0)
    {
        WLT_MFST_LOG_ERR("Read error on %s: %s", path, strerror(saved_errno));
        return WLT_MANIFEST_ERR_IO;
    }

    wlt_manifest_error_t e = parse_manifest_text(buf, m);
    if (e != WLT_MANIFEST_OK)
        return e;

    return wlt_manifest_validate(m);
}

wlt_manifest_error_t
wlt_manifest_parse_seccomp_groups(const char *list, uint32_t *out_mask)
{
    if (list == NULL || out_mask == NULL)
        return WLT_MANIFEST_ERR_INVALID_ARG;

    char buf[256];
    copy_bounded(buf, sizeof(buf), list);

    uint32_t mask = 0;
    char *tok = strtok(buf, ", \t");
    while (tok != NULL)
    {
        if (strcmp(tok, "basic") == 0)
            mask |= (1u << WLT_SECCOMP_GRP_BASIC);
        else if (strcmp(tok, "network") == 0)
            mask |= (1u << WLT_SECCOMP_GRP_NETWORK);
        else if (strcmp(tok, "filesystem") == 0)
            mask |= (1u << WLT_SECCOMP_GRP_FILESYSTEM);
        else if (strcmp(tok, "process") == 0)
            mask |= (1u << WLT_SECCOMP_GRP_PROCESS);
        else if (strcmp(tok, "signal") == 0)
            mask |= (1u << WLT_SECCOMP_GRP_SIGNAL);
        else if (strcmp(tok, "ipc") == 0)
            mask |= (1u << WLT_SECCOMP_GRP_IPC);
        else
        {
            WLT_MFST_LOG_ERR("Unknown seccomp group: %s", tok);
            return WLT_MANIFEST_ERR_PARSE;
        }
        tok = strtok(NULL, ", \t");
    }
    *out_mask = mask;
    return WLT_MANIFEST_OK;
}

const char *
wlt_manifest_net_str(wlt_manifest_net_mode_t m)
{
    switch (m)
    {
        case WLT_NET_TOR_ONLY: return "tor-only";
        case WLT_NET_DIRECT:   return "direct";
        case WLT_NET_NONE:     return "none";
        default:               return "?";
    }
}

const char *
wlt_manifest_fs_str(wlt_manifest_fs_mode_t m)
{
    switch (m)
    {
        case WLT_FS_NONE:    return "none";
        case WLT_FS_HOME_RO: return "home:ro";
        case WLT_FS_HOME_RW: return "home:rw";
        case WLT_FS_FULL:    return "full";
        default:             return "?";
    }
}

const char *
wlt_manifest_perm_str(wlt_manifest_perm_t p)
{
    switch (p)
    {
        case WLT_PERM_ALLOW: return "allow";
        case WLT_PERM_ASK:   return "ask";
        case WLT_PERM_DENY:  return "deny";
        default:             return "?";
    }
}
