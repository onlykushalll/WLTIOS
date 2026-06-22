/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wlt-tor-mgr: Circuit Viewer
 *
 * Reads the live circuit set from Tor (GETINFO circuit-status), resolves each
 * relay in a path to its descriptor (GETINFO ns/id/<fp>) for nickname / IPv4 /
 * consensus flags, and maps the IPv4 to an ISO country code via
 * GETINFO ip-to-country/<ip>.
 *
 * Per the spec we do NOT log circuit contents anywhere persistent; the data
 * lives only in the returned, caller-owned array.
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
#include <time.h>

const char *wlt_tor_status_to_str(wlt_tor_status_t s)
{
    switch (s) {
    case WLT_TOR_STATUS_ACTIVE:     return "active";
    case WLT_TOR_STATUS_CONNECTING: return "connecting";
    case WLT_TOR_STATUS_DOWN:       return "down";
    default:                        return "unknown";
    }
}

const char *wlt_tor_killswitch_to_str(wlt_tor_killswitch_t k)
{
    switch (k) {
    case WLT_TOR_KS_TRIGGERED:  return "triggered";
    case WLT_TOR_KS_RECOVERING: return "recovering";
    case WLT_TOR_KS_NORMAL:     return "normal";
    default:                    return "unknown";
    }
}

const char *wlt_tor_node_flag_str(uint32_t flags, char *buf, size_t len)
{
    static const struct { uint32_t bit; const char *name; } tab[] = {
        { WLT_TOR_NODE_GUARD,     "Guard"     },
        { WLT_TOR_NODE_EXIT,      "Exit"      },
        { WLT_TOR_NODE_MIDDLE,    "Middle"    },
        { WLT_TOR_NODE_AUTHORITY, "Authority" },
        { WLT_TOR_NODE_FAST,      "Fast"      },
        { WLT_TOR_NODE_STABLE,    "Stable"    },
        { WLT_TOR_NODE_RUNNING,   "Running"   },
        { WLT_TOR_NODE_V2DIR,     "V2Dir"     },
        { WLT_TOR_NODE_HSDIR,     "HSDir"     },
    };
    size_t off = 0;
    buf[0] = '\0';
    for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
        if (!(flags & tab[i].bit))
            continue;
        size_t need = strlen(tab[i].name) + (off ? 1 : 0);
        if (off + need + 1 >= len)
            break;
        off += (size_t)snprintf(buf + off, len - off, "%s%s",
                                off ? " " : "", tab[i].name);
    }
    return buf;
}

static uint32_t parse_flags(const char *s)
{
    uint32_t f = 0;
    const char *p = s;
    while (*p) {
        const char *start = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        size_t len = (size_t)(p - start);
#define MATCH(tok, bit) \
    if (len == sizeof(tok) - 1 && strncasecmp(start, tok, len) == 0) f |= (bit)
        MATCH("guard",     WLT_TOR_NODE_GUARD);
        else MATCH("exit",      WLT_TOR_NODE_EXIT);
        else MATCH("fast",      WLT_TOR_NODE_FAST);
        else MATCH("stable",    WLT_TOR_NODE_STABLE);
        else MATCH("running",   WLT_TOR_NODE_RUNNING);
        else MATCH("v2dir",     WLT_TOR_NODE_V2DIR);
        else MATCH("hsdir",     WLT_TOR_NODE_HSDIR);
        else MATCH("authority", WLT_TOR_NODE_AUTHORITY);
#undef MATCH
        while (*p && isspace((unsigned char)*p))
            p++;
    }
    return f;
}

/* Parse one "$fingerprint~nickname" (or "$fingerprint" / "~nickname") path
 * element into the node struct (country/flags filled later). */
static void parse_path_elem(const char *elem, wlt_tor_node_t *node)
{
    const char *fp = elem;
    memset(node, 0, sizeof(*node));
    if (*fp == '$')
        fp++;
    const char *tilde = strchr(elem, '~');
    if (tilde) {
        size_t fp_len = (size_t)(tilde - fp);
        if (fp_len >= sizeof(node->fingerprint))
            fp_len = sizeof(node->fingerprint) - 1;
        memcpy(node->fingerprint, fp, fp_len);
        node->fingerprint[fp_len] = '\0';
        const char *nick = tilde + 1;
        size_t nlen = strlen(nick);
        if (nlen >= sizeof(node->nickname))
            nlen = sizeof(node->nickname) - 1;
        memcpy(node->nickname, nick, nlen);
        node->nickname[nlen] = '\0';
    } else {
        size_t flen = strlen(fp);
        if (flen >= sizeof(node->fingerprint))
            flen = sizeof(node->fingerprint) - 1;
        memcpy(node->fingerprint, fp, flen);
        node->fingerprint[flen] = '\0';
    }
}

/* Pull the descriptor for one fingerprint and fill nickname/ipv4/flags. */
static int lookup_one_node(wlt_tor_control_t *ctl, wlt_tor_node_t *node)
{
    char key[128];
    char reply[WLT_TOR_MAX_REPLY];
    char cmd[WLT_TOR_MAX_LINE];
    int rc;

    /* Country via ip-to-country requires an IP first; we get it from ns. */
    snprintf(key, sizeof(key), "ns/id/$%s", node->fingerprint);
    rc = wlt_tor_control_command(ctl, key, reply, sizeof(reply));
    if (rc != 250 || !reply[0])
        return -1;

    /* ns body: first data line begins with "r <nick> ... <ip> <orport> <dirport>"
     * followed by an "s <flags...>" line. */
    char *rline = strstr(reply, "\nr ");
    if (!rline)
        rline = (reply[0] == 'r') ? reply : NULL;
    else
        rline += 1;
    if (rline) {
        char nick[WLT_TOR_MAX_NICKNAME] = "";
        char ip[WLT_TOR_MAX_ADDR] = "";
        /* "r <nick> <idhash> <published> <ip> <orport> <dirport>" */
        if (sscanf(rline, "r %31s %*s %*s %127s", nick, ip) >= 1) {
            if (nick[0] && !node->nickname[0]) {
                snprintf(node->nickname, sizeof(node->nickname), "%s", nick);
            }
            if (ip[0])
                snprintf(node->ipv4, sizeof(node->ipv4), "%s", ip);
        }
    }
    char *sline = strstr(reply, "\ns ");
    if (sline) {
        sline += 3;
        node->flags = parse_flags(sline);
    }

    /* Resolve country if we have an IPv4. */
    if (node->ipv4[0]) {
        snprintf(cmd, sizeof(cmd), "GETINFO ip-to-country/%s", node->ipv4);
        char creply[256];
        rc = wlt_tor_control_command(ctl, cmd, creply, sizeof(creply));
        if (rc == 250) {
            const char *p = strstr(creply, "ip-to-country=");
            if (p) {
                p += strlen("ip-to-country=");
                size_t i = 0;
                while (p[i] && p[i] != '\n' && p[i] != ' ' &&
                       i + 1 < sizeof(node->country)) {
                    node->country[i] = (char)toupper((unsigned char)p[i]);
                    i++;
                }
                node->country[i] = '\0';
            }
        }
    }
    return 0;
}

int wlt_tor_circuit_lookup_nodes(wlt_tor_control_t *ctl, wlt_tor_circuit_t *circ)
{
    if (!ctl || !circ)
        return -EINVAL;
    for (size_t i = 0; i < circ->node_count; i++)
        lookup_one_node(ctl, &circ->nodes[i]);
    return 0;
}

/* Parse a single circuit-status line into the circuit struct. */
static int parse_circuit_line(const char *line, wlt_tor_circuit_t *circ)
{
    char idbuf[32];
    char statusbuf[16];
    char path[WLT_TOR_MAX_LINE];
    char purpose[32] = "";
    char *save = NULL;

    memset(circ, 0, sizeof(*circ));
    if (sscanf(line, "%31s %15s %1023s", idbuf, statusbuf, path) < 3)
        return -1;

    circ->id = (uint32_t)strtoul(idbuf, NULL, 10);
    snprintf(circ->status, sizeof(circ->status), "%s", statusbuf);

    /* Path is comma-separated relay specs. */
    char *tok = strtok_r(path, ",", &save);
    while (tok && circ->node_count < WLT_TOR_MAX_NODES) {
        parse_path_elem(tok, &circ->nodes[circ->node_count]);
        circ->node_count++;
        tok = strtok_r(NULL, ",", &save);
    }

    /* Scan the remainder of the line for PURPOSE= and TIME_CREATED=. */
    const char *p = line;
    while ((p = strstr(p, "PURPOSE="))) {
        p += strlen("PURPOSE=");
        size_t i = 0;
        while (p[i] && p[i] != ' ' && i + 1 < sizeof(purpose)) {
            purpose[i] = p[i];
            i++;
        }
        purpose[i] = '\0';
        break;
    }
    snprintf(circ->purpose, sizeof(circ->purpose), "%s", purpose);
    circ->built_at = time(NULL);
    return 0;
}

int wlt_tor_circuits_refresh(wlt_tor_control_t *ctl, wlt_tor_circuit_t *out,
                             size_t max, size_t *count)
{
    char reply[WLT_TOR_MAX_REPLY];
    char *save = NULL;
    char *line;
    size_t n = 0;
    int rc;

    if (!ctl || !out || !count || !max)
        return -EINVAL;
    *count = 0;

    rc = wlt_tor_control_command(ctl, "GETINFO circuit-status", reply,
                                 sizeof(reply));
    if (rc != 250 || !reply[0]) {
        wlt_log_dbg("circuit-status unavailable (rc=%d)", rc);
        return rc < 0 ? rc : -EAGAIN;
    }

    /* reply = "circuit-status=\n<circuit>...\nOK" */
    char *p = strstr(reply, "circuit-status=\n");
    if (!p)
        p = strstr(reply, "circuit-status=");
    if (!p)
        return -ENOENT;
    p = strchr(p, '\n');
    if (!p)
        return -ENOENT;
    p++;

    char *block = strdup(p);
    if (!block)
        return -ENOMEM;
    /* Strip the trailing "OK" sentinel line if present. */
    char *ok = strstr(block, "\nOK");
    if (ok)
        *ok = '\0';

    line = strtok_r(block, "\n", &save);
    while (line && n < max) {
        if (parse_circuit_line(line, &out[n]) == 0) {
            /* Only expose live circuits the user cares about. */
            if (strcasecmp(out[n].status, "CLOSED") != 0 &&
                strcasecmp(out[n].status, "FAILED") != 0) {
                n++;
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }
    free(block);

    /* Enrich each circuit with descriptor/country info. We tolerate per-node
     * lookup failures so a partial Tor consensus still renders something. */
    for (size_t i = 0; i < n; i++)
        wlt_tor_circuit_lookup_nodes(ctl, &out[i]);

    *count = n;
    wlt_log_dbg("refreshed circuits: %zu", n);
    return 0;
}
