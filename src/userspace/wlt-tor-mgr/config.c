/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wlt-tor-mgr: Config Parsing
 *
 * Reads /etc/wlt/tor.conf (INI-style) into wlt_tor_config_t and writes it
 * back. Recognised sections:
 *
 *   [wlt-tor-mgr]
 *     socks_port = 9050
 *     dns_port   = 5353
 *     control_port = 9051
 *     control_host = 127.0.0.1
 *     status_interval = 1
 *     cookie_path = /var/lib/tor/control_auth_cookie
 *     cookie_jar  = /var/lib/wlt/cookie-jar
 *
 *   [bridges]
 *     enabled = no
 *     bridge  = obfs4 192.0.2.1:443 CDF2... cert=... iat-mode=0
 *     bridge  = 192.0.2.2:443 CDF2...
 *
 *   [exit]
 *     mode    = auto        ; or "country"
 *     country = DE
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "tor_mgr.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

static void strip_comment(char *s)
{
    /* Remove trailing ';' or '#' comment, respecting that '#' may legitimately
     * appear nowhere in our keys/values. Simple first-occurrence cut. */
    for (char *p = s; *p; p++) {
        if (*p == ';' || *p == '#') {
            *p = '\0';
            return;
        }
    }
}

static bool parse_bool(const char *v, bool *out)
{
    if (!v)
        return false;
    if (!strcasecmp(v, "yes") || !strcasecmp(v, "true") || !strcasecmp(v, "1") || !strcasecmp(v, "on")) {
        *out = true;
        return true;
    }
    if (!strcasecmp(v, "no") || !strcasecmp(v, "false") || !strcasecmp(v, "0") || !strcasecmp(v, "off")) {
        *out = false;
        return true;
    }
    return false;
}

int wlt_tor_config_init_defaults(wlt_tor_config_t *cfg)
{
    if (!cfg)
        return -EINVAL;
    memset(cfg, 0, sizeof(*cfg));
    cfg->socks_port       = 9050;
    cfg->dns_port         = 5353;
    cfg->control_port     = 9051;
    snprintf(cfg->control_host, sizeof(cfg->control_host), "127.0.0.1");
    cfg->status_interval  = WLT_TOR_DEFAULT_INTERVAL;
    cfg->bridges_enabled  = false;
    cfg->exit_mode        = WLT_TOR_EXIT_AUTO;
    snprintf(cfg->config_path, sizeof(cfg->config_path), "%s", WLT_TOR_CONFIG_PATH);
    snprintf(cfg->cookie_path, sizeof(cfg->cookie_path), "%s", WLT_TOR_COOKIE_PATH);
    snprintf(cfg->cookie_jar, sizeof(cfg->cookie_jar), "%s", WLT_TOR_COOKIE_JAR);
    return 0;
}

enum section { SEC_NONE, SEC_MGR, SEC_BRIDGES, SEC_EXIT };

static enum section parse_section(const char *line)
{
    if (strcasecmp(line, "wlt-tor-mgr") == 0) return SEC_MGR;
    if (strcasecmp(line, "bridges") == 0)     return SEC_BRIDGES;
    if (strcasecmp(line, "exit") == 0)        return SEC_EXIT;
    return SEC_NONE;
}

static void apply_kv(wlt_tor_config_t *cfg, enum section sec,
                     const char *k, const char *v)
{
    if (!k || !v)
        return;
    switch (sec) {
    case SEC_MGR:
        if (!strcasecmp(k, "socks_port"))
            cfg->socks_port = (uint16_t)strtoul(v, NULL, 10);
        else if (!strcasecmp(k, "dns_port"))
            cfg->dns_port = (uint16_t)strtoul(v, NULL, 10);
        else if (!strcasecmp(k, "control_port"))
            cfg->control_port = (uint16_t)strtoul(v, NULL, 10);
        else if (!strcasecmp(k, "control_host"))
            snprintf(cfg->control_host, sizeof(cfg->control_host), "%s", v);
        else if (!strcasecmp(k, "status_interval"))
            cfg->status_interval = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcasecmp(k, "cookie_path"))
            snprintf(cfg->cookie_path, sizeof(cfg->cookie_path), "%s", v);
        else if (!strcasecmp(k, "cookie_jar"))
            snprintf(cfg->cookie_jar, sizeof(cfg->cookie_jar), "%s", v);
        break;
    case SEC_BRIDGES:
        if (!strcasecmp(k, "enabled")) {
            parse_bool(v, &cfg->bridges_enabled);
        } else if (!strcasecmp(k, "bridge")) {
            wlt_tor_bridge_t br;
            if (wlt_tor_bridge_parse(v, &br) == 0) {
                if (cfg->bridge_count < WLT_TOR_MAX_BRIDGES)
                    cfg->bridges[cfg->bridge_count++] = br;
                else
                    wlt_log_wrn("bridge table full; ignoring %s", v);
            } else {
                wlt_log_wrn("ignoring malformed bridge line: %s", v);
            }
        }
        break;
    case SEC_EXIT:
        if (!strcasecmp(k, "mode")) {
            if (!strcasecmp(v, "country"))
                cfg->exit_mode = WLT_TOR_EXIT_COUNTRY;
            else
                cfg->exit_mode = WLT_TOR_EXIT_AUTO;
        } else if (!strcasecmp(k, "country")) {
            size_t i = 0;
            while (v[i] && i + 1 < sizeof(cfg->exit_country)) {
                cfg->exit_country[i] = (char)toupper((unsigned char)v[i]);
                i++;
            }
            cfg->exit_country[i] = '\0';
        }
        break;
    default:
        break;
    }
}

int wlt_tor_config_load(wlt_tor_config_t *cfg, const char *path)
{
    FILE *f;
    char line[WLT_TOR_MAX_LINE];
    enum section sec = SEC_NONE;

    if (!cfg)
        return -EINVAL;
    wlt_tor_config_init_defaults(cfg);
    if (path && *path)
        snprintf(cfg->config_path, sizeof(cfg->config_path), "%s", path);
    else
        path = cfg->config_path;

    f = fopen(path, "re");
    if (!f) {
        if (errno == ENOENT) {
            wlt_log_inf("no config at %s; using defaults", path);
            return 0;
        }
        wlt_log_err("config open(%s): %s", path, strerror(errno));
        return -errno;
    }
    while (fgets(line, sizeof(line), f)) {
        strip_comment(line);
        char *s = trim(line);
        if (!*s)
            continue;
        if (s[0] == '[') {
            char *end = strchr(s, ']');
            if (!end)
                continue;
            *end = '\0';
            sec = parse_section(trim(s + 1));
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *k = trim(s);
        char *v = trim(eq + 1);
        apply_kv(cfg, sec, k, v);
    }
    fclose(f);
    wlt_log_inf("config loaded from %s (%zu bridges, exit=%s%s)",
                path, cfg->bridge_count,
                cfg->exit_mode == WLT_TOR_EXIT_COUNTRY ? "country:" : "auto",
                cfg->exit_mode == WLT_TOR_EXIT_COUNTRY ? cfg->exit_country : "");
    return 0;
}

int wlt_tor_config_save(const wlt_tor_config_t *cfg, const char *path)
{
    FILE *f;
    char spec[WLT_TOR_MAX_LINE];

    if (!cfg)
        return -EINVAL;
    if (!path || !*path)
        path = cfg->config_path;

    f = fopen(path, "we");
    if (!f) {
        wlt_log_err("config save(%s): %s", path, strerror(errno));
        return -errno;
    }
    fprintf(f, "# WLTIOS Tor manager config - generated by %s\n",
            WLT_TOR_MGR_NAME);
    fprintf(f, "[wlt-tor-mgr]\n");
    fprintf(f, "socks_port = %u\n", (unsigned)cfg->socks_port);
    fprintf(f, "dns_port = %u\n", (unsigned)cfg->dns_port);
    fprintf(f, "control_port = %u\n", (unsigned)cfg->control_port);
    fprintf(f, "control_host = %s\n", cfg->control_host);
    fprintf(f, "status_interval = %u\n", (unsigned)cfg->status_interval);
    fprintf(f, "cookie_path = %s\n", cfg->cookie_path);
    fprintf(f, "cookie_jar = %s\n", cfg->cookie_jar);

    fprintf(f, "\n[bridges]\n");
    fprintf(f, "enabled = %s\n", cfg->bridges_enabled ? "yes" : "no");
    for (size_t i = 0; i < cfg->bridge_count; i++) {
        /* Reuse the bridge serializer from bridges.c indirectly via a local
         * rebuild so this translation unit stays self-contained. */
        const wlt_tor_bridge_t *br = &cfg->bridges[i];
        size_t off = 0;
        spec[0] = '\0';
        if (br->pt_type[0])
            off += (size_t)snprintf(spec + off, sizeof(spec) - off, "%s ", br->pt_type);
        off += (size_t)snprintf(spec + off, sizeof(spec) - off, "%s", br->addr);
        if (br->fingerprint[0])
            off += (size_t)snprintf(spec + off, sizeof(spec) - off, " %s", br->fingerprint);
        if (br->pt_args[0])
            off += (size_t)snprintf(spec + off, sizeof(spec) - off, " %s", br->pt_args);
        fprintf(f, "bridge = %s\n", spec);
    }

    fprintf(f, "\n[exit]\n");
    fprintf(f, "mode = %s\n",
            cfg->exit_mode == WLT_TOR_EXIT_COUNTRY ? "country" : "auto");
    if (cfg->exit_mode == WLT_TOR_EXIT_COUNTRY)
        fprintf(f, "country = %s\n", cfg->exit_country);
    fclose(f);
    return 0;
}
