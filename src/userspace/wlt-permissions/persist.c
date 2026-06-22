/**
 * @file persist.c
 * @brief Grant-state persistence for WLT-Permissions
 * @module wlt-permissions/persist
 * @purpose Saves/loads effective per-app permission grants and rebuilds
 *          the manager from on-disk manifests (spec Â§Error recovery).
 * @dependencies: manifest.h, permissions.h
 * @tested_by: tests/userspace/permissions_persist_test.c
 * @spec: docs/specs/wlt-permissions.md
 *
 * Split out of permissions.c to keep each file under the 500-line limit
 * (CODING_STYLE Â§1). Grant state lives in <grants_dir>/<app>.ini:
 *
 *     [meta]
 *     app = firefox
 *     updated_at = 1719067267
 *     updated_by = user
 *     [grants]
 *     network = tor-only
 *     camera = allow
 */

#include "permissions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define WLT_PP_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-permissions/persist][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_PP_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-permissions/persist][WRN] " fmt "\n", ##__VA_ARGS__)

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

/* Trim in place, shifting left so the result starts at the buffer. */
static void
trim(char *s)
{
    char *start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

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
    const char *vs = eq + 1;
    int vlen = (int)strlen(vs);
    if (vlen >= val_max)
        vlen = val_max - 1;
    memcpy(val, vs, (size_t)vlen);
    val[vlen] = '\0';
    trim(key);
    trim(val);
    return 0;
}

/* Build <grants_dir>/<app>.ini<suffix> into @out. app_id and suffix are
 * copied into fixed buffers first so the compiler can prove the path
 * fits WLT_PERMS_GRANT_PATH_MAX without truncation. */
static wlt_perms_error_t
grant_file_path(const wlt_PermsManager *m, const char *app_id,
                const char *suffix, char *out, size_t out_max)
{
    char id[WLT_PERMS_MAX_NAME];
    char sfx[8];
    copy_bounded(id, sizeof(id), app_id ? app_id : "");
    copy_bounded(sfx, sizeof(sfx), suffix ? suffix : "");
    int n = snprintf(out, out_max, "%s/%s.ini%s", m->grants_dir, id, sfx);
    if (n < 0 || (size_t)n >= out_max)
        return WLT_PERMS_ERR_TOO_LARGE;
    return WLT_PERMS_OK;
}

/* âââ Save âââââââââââââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_save(const wlt_PermsManager *m, const char *app_id)
{
    if (m == NULL || app_id == NULL)
        return WLT_PERMS_ERR_INVALID_ARG;

    /* Const-correct lookup: save only reads the app record. */
    const wlt_PermsApp *app = NULL;
    for (size_t i = 0; i < m->app_count; i++)
    {
        if (strcmp(m->apps[i].id, app_id) == 0)
        {
            app = &m->apps[i];
            break;
        }
    }
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;

    char path[WLT_PERMS_GRANT_PATH_MAX];
    char tmp[WLT_PERMS_GRANT_PATH_MAX];
    wlt_perms_error_t pe = grant_file_path(m, app_id, ".tmp", tmp, sizeof(tmp));
    if (pe != WLT_PERMS_OK)
        return pe;
    pe = grant_file_path(m, app_id, "", path, sizeof(path));
    if (pe != WLT_PERMS_OK)
        return pe;

    FILE *f = fopen(tmp, "w");
    if (f == NULL)
    {
        WLT_PP_LOG_ERR("open(%s): %s", tmp, strerror(errno));
        return WLT_PERMS_ERR_IO;
    }
    fprintf(f, "# wlt-permissions grant state â do not edit by hand\n");
    fprintf(f, "[meta]\napp = %s\nupdated_at = %llu\nupdated_by = %s\n",
            app->id, (unsigned long long)app->updated_at, app->updated_by);
    fprintf(f, "[grants]\n");
    for (int i = 0; i < WLT_PERM_COUNT; i++)
    {
        /* Persist declared perms only; undeclared carry nothing across
         * reboot and would only clutter the file. */
        if ((app->declared_mask & (1u << i)) == 0)
            continue;
        fprintf(f, "%s = %s\n", wlt_perms_id_str((wlt_perms_id_t)i),
                wlt_perms_value_str((wlt_perms_id_t)i, app->grant[i]));
    }
    fflush(f);
    if (ferror(f))
    {
        WLT_PP_LOG_ERR("write(%s): %s", tmp, strerror(errno));
        fclose(f);
        return WLT_PERMS_ERR_IO;
    }
    fclose(f);
    if (rename(tmp, path) != 0)  /* atomic publish */
    {
        WLT_PP_LOG_ERR("rename(%s): %s", path, strerror(errno));
        return WLT_PERMS_ERR_IO;
    }
    return WLT_PERMS_OK;
}

/* âââ Load âââââââââââââââââââââââââââââââââââââââââââââ */

static wlt_perms_error_t
parse_grant_text(wlt_PermsApp *app, const char *text)
{
    char section[32] = "";
    const char *ptr = text;
    char line[WLT_PERMS_MAX_LINE];

    while (*ptr)
    {
        size_t i = 0;
        while (*ptr && *ptr != '\n' && i < sizeof(line) - 1)
            line[i++] = *ptr++;
        line[i] = '\0';
        if (*ptr == '\n')
            ptr++;
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if (line[0] == '[')
        {
            char *end = strchr(line, ']');
            if (end == NULL)
                return WLT_PERMS_ERR_PARSE;
            *end = '\0';
            copy_bounded(section, sizeof(section), line + 1);
            continue;
        }

        char key[128], val[WLT_PERMS_MAX_LINE];
        if (parse_kv(line, key, sizeof(key), val, sizeof(val)) != 0)
            continue;

        if (strcmp(section, "meta") == 0)
        {
            if (strcmp(key, "updated_at") == 0)
                app->updated_at = (uint64_t)strtoull(val, NULL, 10);
            else if (strcmp(key, "updated_by") == 0)
                copy_bounded(app->updated_by, sizeof(app->updated_by), val);
        }
        else if (strcmp(section, "grants") == 0)
        {
            wlt_perms_id_t id = wlt_perms_id_from_str(key);
            /* Never let a stale file grant an undeclared permission. */
            if (id != WLT_PERM_COUNT &&
                (app->declared_mask & (1u << id)) != 0)
            {
                wlt_perms_value_t v;
                if (wlt_perms_value_from_str(wlt_perms_id_kind(id), val, &v)
                    != WLT_PERMS_MFST_OK)
                    return WLT_PERMS_ERR_PARSE;
                app->grant[id] = v;
            }
        }
    }
    return WLT_PERMS_OK;
}

wlt_perms_error_t
wlt_perms_load(wlt_PermsManager *m, const char *app_id)
{
    if (m == NULL || app_id == NULL)
        return WLT_PERMS_ERR_INVALID_ARG;
    wlt_PermsApp *app = wlt_perms_find(m, app_id);
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;

    char path[WLT_PERMS_GRANT_PATH_MAX];
    wlt_perms_error_t pe = grant_file_path(m, app_id, "", path, sizeof(path));
    if (pe != WLT_PERMS_OK)
        return pe;

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return (errno == ENOENT) ? WLT_PERMS_ERR_NOT_FOUND : WLT_PERMS_ERR_IO;

    char buf[WLT_PERMS_GRANT_FILE_MAX];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    int saved = ferror(f) ? errno : 0;
    fclose(f);
    if (saved != 0)
        return WLT_PERMS_ERR_IO;
    return parse_grant_text(app, buf);
}

/* âââ Rebuild ââââââââââââââââââââââââââââââââââââââââââ */

static bool
has_suffix(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

wlt_perms_error_t
wlt_perms_rebuild(wlt_PermsManager *m, const char *apps_dir)
{
    if (m == NULL || apps_dir == NULL)
        return WLT_PERMS_ERR_INVALID_ARG;
    if (strlen(apps_dir) >= WLT_PERMS_MAX_PATH)
        return WLT_PERMS_ERR_TOO_LARGE;

    /* Copy apps_dir into a fixed buffer so the compiler can prove the
     * manifest path fits; the length guard above makes this lossless. */
    char dir[WLT_PERMS_MAX_PATH];
    copy_bounded(dir, sizeof(dir), apps_dir);

    DIR *d = opendir(apps_dir);
    if (d == NULL)
    {
        WLT_PP_LOG_ERR("opendir(%s): %s", apps_dir, strerror(errno));
        return WLT_PERMS_ERR_IO;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL)
    {
        if (!has_suffix(de->d_name, WLT_PERMS_MANIFEST_SUFFIX))
            continue;

        char path[WLT_PERMS_MAX_PATH + 256 + 8];
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);

        wlt_PermsManifest mfst;
        if (wlt_perms_manifest_load(path, &mfst) != WLT_PERMS_MFST_OK)
        {
            WLT_PP_LOG_ERR("skip %s: manifest parse failed", path);
            continue;
        }

        wlt_perms_error_t e = wlt_perms_register(m, &mfst);
        if (e == WLT_PERMS_ERR_FULL)
        {
            WLT_PP_LOG_WRN("app table full at %zu â ignoring remaining",
                           m->app_count);
            break;
        }
        if (e != WLT_PERMS_OK)
        {
            WLT_PP_LOG_ERR("register %s failed: %s", mfst.name,
                           wlt_perms_error_str(e));
            continue;
        }
        /* Overlay saved grants; a missing file keeps the defaults. */
        e = wlt_perms_load(m, mfst.name);
        if (e != WLT_PERMS_OK && e != WLT_PERMS_ERR_NOT_FOUND)
            WLT_PP_LOG_WRN("load grants for %s: %s", mfst.name,
                           wlt_perms_error_str(e));
    }
    closedir(d);
    return WLT_PERMS_OK;
}
