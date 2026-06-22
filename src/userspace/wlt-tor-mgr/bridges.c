/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wlt-tor-mgr: Bridge Configuration
 *
 * Manages the in-memory bridge table (part of wlt_tor_config_t) and applies it
 * to the running Tor via SETCONF. Bridges are pluggable-transport relays used
 * for censorship circumvention; when enabled Tor refuses direct directory
 * connections.
 *
 * Bridge spec grammar (per torrc):
 *   [<pt_type> ]<host>:<port> [<fingerprint>] [<pt_args>...]
 * e.g.
 *   obfs4 192.0.2.1:443 CDF2E852BF539B82BD10E27E9115A31734E378C2 cert=... iat-mode=0
 *   192.0.2.2:443 CDF2E852BF539B82BD10E27E9115A31734E378C2
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

/* Is this token a host:port pair (contains a ':' and no spaces)? */
static bool is_hostport(const char *tok)
{
    return strchr(tok, ':') != NULL;
}

/* Is this token a 40-hex fingerprint, optionally prefixed with '$'? */
static bool is_fingerprint(const char *tok)
{
    const char *p = tok;
    if (*p == '$')
        p++;
    if (strlen(p) != 40)
        return false;
    for (size_t i = 0; i < 40; i++)
        if (!isxdigit((unsigned char)p[i]))
            return false;
    return true;
}

int wlt_tor_bridge_parse(const char *line, wlt_tor_bridge_t *out)
{
    char buf[WLT_TOR_MAX_LINE];
    char *save = NULL;
    char *tok;
    int tokens_seen = 0;
    bool have_pt = false;

    if (!line || !out)
        return -EINVAL;
    memset(out, 0, sizeof(*out));

    snprintf(buf, sizeof(buf), "%s", line);
    /* Skip a leading "bridge=" if the caller passed a raw config line. */
    char *start = buf;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (strncasecmp(start, "bridge=", 7) == 0)
        start += 7;
    while (*start && isspace((unsigned char)*start))
        start++;

    tok = strtok_r(start, " \t", &save);
    while (tok) {
        if (tokens_seen == 0 && !is_hostport(tok)) {
            /* First token is a pluggable transport type. */
            snprintf(out->pt_type, sizeof(out->pt_type), "%s", tok);
            have_pt = true;
        } else if (is_hostport(tok) && !out->addr[0]) {
            snprintf(out->addr, sizeof(out->addr), "%s", tok);
        } else if (is_fingerprint(tok) && !out->fingerprint[0]) {
            const char *fp = (tok[0] == '$') ? tok + 1 : tok;
            snprintf(out->fingerprint, sizeof(out->fingerprint), "%s", fp);
        } else {
            /* Everything else is a transport argument (key=value). */
            if (out->pt_args[0])
                strncat(out->pt_args, " ", sizeof(out->pt_args) - strlen(out->pt_args) - 1);
            strncat(out->pt_args, tok, sizeof(out->pt_args) - strlen(out->pt_args) - 1);
        }
        tokens_seen++;
        tok = strtok_r(NULL, " \t", &save);
    }
    (void)have_pt;
    if (!out->addr[0])
        return -EINVAL;
    out->enabled = true;
    return 0;
}

/* Build a canonical Tor bridge line, e.g.
 *   obfs4 192.0.2.1:443 CDF2... cert=... iat-mode=0
 */
static int bridge_to_spec(const wlt_tor_bridge_t *br, char *out, size_t out_len)
{
    size_t off = 0;
    out[0] = '\0';
    if (br->pt_type[0])
        off += (size_t)snprintf(out + off, out_len - off, "%s ", br->pt_type);
    off += (size_t)snprintf(out + off, out_len - off, "%s", br->addr);
    if (br->fingerprint[0])
        off += (size_t)snprintf(out + off, out_len - off, " %s", br->fingerprint);
    if (br->pt_args[0])
        off += (size_t)snprintf(out + off, out_len - off, " %s", br->pt_args);
    return 0;
}

int wlt_tor_bridges_add(wlt_tor_config_t *cfg, const wlt_tor_bridge_t *br)
{
    if (!cfg || !br || !br->addr[0])
        return -EINVAL;
    if (cfg->bridge_count >= WLT_TOR_MAX_BRIDGES)
        return -ENOSPC;
    /* De-dupe by address. */
    for (size_t i = 0; i < cfg->bridge_count; i++) {
        if (strcmp(cfg->bridges[i].addr, br->addr) == 0)
            return -EEXIST;
    }
    cfg->bridges[cfg->bridge_count++] = *br;
    cfg->bridges_enabled = true;
    wlt_log_inf("bridge added: %s%s%s",
                br->pt_type[0] ? br->pt_type : "",
                br->pt_type[0] ? " " : "", br->addr);
    return 0;
}

int wlt_tor_bridges_remove(wlt_tor_config_t *cfg, const char *addr)
{
    if (!cfg || !addr)
        return -EINVAL;
    for (size_t i = 0; i < cfg->bridge_count; i++) {
        if (strcmp(cfg->bridges[i].addr, addr) == 0) {
            cfg->bridges[i] = cfg->bridges[cfg->bridge_count - 1];
            cfg->bridge_count--;
            if (cfg->bridge_count == 0)
                cfg->bridges_enabled = false;
            wlt_log_inf("bridge removed: %s", addr);
            return 0;
        }
    }
    return -ENOENT;
}

int wlt_tor_bridges_list(const wlt_tor_config_t *cfg, wlt_tor_bridge_t *out,
                         size_t max, size_t *count)
{
    if (!cfg || !count)
        return -EINVAL;
    *count = cfg->bridge_count;
    if (!out || max == 0)
        return 0;
    size_t n = cfg->bridge_count < max ? cfg->bridge_count : max;
    for (size_t i = 0; i < n; i++)
        out[i] = cfg->bridges[i];
    return 0;
}

/* Escape a value into a Tor control quoted string. Doubles backslashes and
 * quotes, then wraps in double quotes. */
static void quote_value(const char *in, char *out, size_t out_len)
{
    size_t o = 0;
    if (out_len < 3) {
        if (out_len)
            out[0] = '\0';
        return;
    }
    out[o++] = '"';
    for (size_t i = 0; in[i] && o + 2 < out_len; i++) {
        if (in[i] == '"' || in[i] == '\\')
            out[o++] = '\\';
        out[o++] = in[i];
    }
    out[o++] = '"';
    out[o] = '\0';
}

int wlt_tor_bridges_apply(wlt_tor_control_t *ctl, const wlt_tor_config_t *cfg)
{
    char spec[WLT_TOR_MAX_LINE];
    char quoted[WLT_TOR_MAX_LINE + 4];
    char cmd[WLT_TOR_MAX_REPLY];
    char reply[WLT_TOR_MAX_REPLY];
    int rc;

    if (!ctl || !cfg)
        return -EINVAL;

    /* Always reset the Bridge list first so removals take effect. */
    rc = wlt_tor_control_resetconf(ctl, "Bridge");
    if (rc < 0)
        return rc;

    if (!cfg->bridges_enabled || cfg->bridge_count == 0) {
        rc = wlt_tor_control_setconf(ctl, "UseBridges", "0");
        return rc;
    }

    rc = wlt_tor_control_setconf(ctl, "UseBridges", "1");
    if (rc < 0)
        return rc;

    /* Set all bridges in a single SETCONF so Tor treats them atomically. */
    size_t off = (size_t)snprintf(cmd, sizeof(cmd), "SETCONF");
    for (size_t i = 0; i < cfg->bridge_count; i++) {
        bridge_to_spec(&cfg->bridges[i], spec, sizeof(spec));
        quote_value(spec, quoted, sizeof(quoted));
        int w = snprintf(cmd + off, sizeof(cmd) - off, " Bridge=%s", quoted);
        if (w < 0 || (size_t)w >= sizeof(cmd) - off) {
            wlt_log_err("bridge list too long for one SETCONF; applying per-bridge");
            /* Fallback: apply each bridge in its own SETCONF (append). */
            char one[64 + WLT_TOR_MAX_LINE];
            snprintf(one, sizeof(one), "SETCONF Bridge=%s", quoted);
            int r2 = wlt_tor_control_command(ctl, one, reply, sizeof(reply));
            if (r2 != 250)
                return -EPROTO;
            off = 0; /* next iteration rebuilds */
            continue;
        }
        off += (size_t)w;
    }
    rc = wlt_tor_control_command(ctl, cmd, reply, sizeof(reply));
    if (rc != 250) {
        wlt_log_err("SETCONF bridges failed: %d %s", rc, reply);
        return -EPROTO;
    }
    wlt_log_inf("applied %zu bridge(s)", cfg->bridge_count);
    return 0;
}
