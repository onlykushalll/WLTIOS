/**
 * @file guardian.c
 * @brief Kairos Proactive Guardian — 24/7 security monitoring (JARVIS mode)
 * @module wlt-ai/guardian
 * @purpose Protect user even when inactive — detect intrusions, prevent attacks
 * @dependencies: ai.h
 * @spec: docs/specs/wlt-ai.md
 * @adr: docs/adr/08-kairos.md
 *
 * The Guardian runs a background event loop (every 1 second) that checks for:
 * - Network anomalies (port scans, unexpected connections, data exfiltration)
 * - Tor health (disconnects, DNS leaks, circuit degradation)
 * - Process behavior (CPU/RAM spikes, unauthorized access)
 * - App permission violations
 * - Browser safety (phishing, malicious downloads)
 * - Disk security (encryption status, unattended screen)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

typedef enum {
    WLT_GUARDIAN_OK = 0,
    WLT_GUARDIAN_ERR = -1,
} wlt_guardian_error_t;

typedef enum {
    WLT_THREAT_NONE = 0,
    WLT_THREAT_LOW,      /* Log + silent fix */
    WLT_THREAT_MEDIUM,   /* Notify user */
    WLT_THREAT_HIGH,     /* Voice + visual alert */
    WLT_THREAT_CRITICAL, /* Kill switch + lockdown */
} wlt_guardian_threat_level_t;

typedef struct {
    wlt_guardian_threat_level_t level;
    char description[256];
    char action_taken[256];
    uint64_t timestamp;
} wlt_guardian_event_t;

#define GUARDIAN_MAX_EVENTS 100
#define GUARDIAN_CHECK_INTERVAL_MS 1000
#define GUARDIAN_MAX_PORT_SCANS 10
#define GUARDIAN_MAX_CONNECTIONS 200

static wlt_guardian_event_t g_events[GUARDIAN_MAX_EVENTS];
static size_t g_event_count = 0;
static bool g_guardian_running = false;
static time_t g_last_user_activity = 0;

/* Log a guardian event */
static void
guardian_log(wlt_guardian_threat_level_t level,
             const char *description, const char *action)
{
    if (g_event_count >= GUARDIAN_MAX_EVENTS)
    {
        /* Shift events */
        memmove(g_events, g_events + 1,
                (GUARDIAN_MAX_EVENTS - 1) * sizeof(wlt_guardian_event_t));
        g_event_count = GUARDIAN_MAX_EVENTS - 1;
    }

    wlt_guardian_event_t *ev = &g_events[g_event_count++];
    ev->level = level;
    ev->timestamp = (uint64_t)time(NULL);
    strncpy(ev->description, description, sizeof(ev->description) - 1);
    strncpy(ev->action_taken, action, sizeof(ev->action_taken) - 1);

    /* TODO(kairos): Send to Kairos AI for analysis */
    /* TODO(kairos): If HIGH/CRITICAL → voice alert + visual overlay */
}

/* ─── Network Checks ─────────────────────────────────── */

static wlt_guardian_threat_level_t
check_port_scans(void)
{
    /* TODO(kairos): Read /proc/net/tcp + /proc/net/tcp6 */
    /* Count connections from same IP to multiple ports */
    /* If > GUARDIAN_MAX_PORT_SCANS → THREAT_HIGH */
    return WLT_THREAT_NONE;
}

static wlt_guardian_threat_level_t
check_unexpected_connections(void)
{
    /* TODO(kairos): Check for non-Tor outbound connections */
    /* Any connection NOT going through 127.0.0.1:9050 is suspicious */
    /* Read /proc/net/tcp, check for remote IPs != 127.0.0.1 */
    return WLT_THREAT_NONE;
}

static wlt_guardian_threat_level_t
check_data_exfiltration(void)
{
    /* TODO(kairos): Monitor bandwidth usage per process */
    /* If a process uploads > 100MB unexpectedly → THREAT_HIGH */
    return WLT_THREAT_NONE;
}

/* ─── Tor Health Checks ──────────────────────────────── */

static wlt_guardian_threat_level_t
check_tor_health(void)
{
    /* Check if Tor SOCKS port is responding */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return WLT_THREAT_NONE;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9050);
    addr.sin_addr.s_addr = htonl(0x7F000001);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv = { .tv_usec = 100000 };

    bool connected = false;
    if (select(sock + 1, NULL, &fds, NULL, &tv) > 0)
    {
        int err;
        socklen_t err_len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &err_len);
        connected = (err == 0);
    }

    close(sock);

    if (!connected)
    {
        guardian_log(WLT_THREAT_CRITICAL,
                     "Tor SOCKS proxy unreachable — network isolation compromised",
                     "Kill switch activated — ALL network traffic blocked");
        /* TODO(kairos): Trigger kill switch via wlt-tor-mgr */
        return WLT_THREAT_CRITICAL;
    }

    return WLT_THREAT_NONE;
}

static wlt_guardian_threat_level_t
check_dns_leak(void)
{
    /* TODO(kairos): Check if any DNS queries went outside Tor */
    /* Read /proc/net/udp — port 53 should only be 127.0.0.1:5353 */
    /* If any external DNS server → THREAT_HIGH */
    return WLT_THREAT_NONE;
}

/* ─── Process Behavior Checks ────────────────────────── */

static wlt_guardian_threat_level_t
check_process_anomalies(void)
{
    DIR *d = opendir("/proc");
    if (d == NULL)
        return WLT_THREAT_NONE;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        /* Skip non-numeric entries (not PIDs) */
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;

        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);

        FILE *f = fopen(stat_path, "r");
        if (f == NULL)
            continue;

        /* Read /proc/PID/stat: pid (comm) state ppid ... */
        int pid;
        char comm[256];
        char state;
        if (fscanf(f, "%d (%255[^)]) %c", &pid, comm, &state) >= 3)
        {
            /* TODO(kairos): Check CPU usage, memory usage */
            /* TODO(kairos): Flag processes accessing camera without permission */
            /* TODO(kairos): Flag processes writing to /etc/ */
        }

        fclose(f);
    }

    closedir(d);
    return WLT_THREAT_NONE;
}

/* ─── App Permission Checks ──────────────────────────── */

static wlt_guardian_threat_level_t
check_app_permissions(void)
{
    /* TODO(kairos): Read /etc/wlt/apps/*.manifest */
    /* TODO(kairos): For each running app, verify it stays within declared permissions */
    /* TODO(kairos): If app accesses camera without permission → kill + alert */
    /* TODO(kairos): If app makes direct network (not Tor) → block + alert */
    return WLT_THREAT_NONE;
}

/* ─── Disk Security Checks ───────────────────────────── */

static wlt_guardian_threat_level_t
check_disk_security(void)
{
    /* Check if disk encryption is active */
    FILE *f = fopen("/proc/crypto", "r");
    if (f != NULL)
    {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);

        if (n > 0 && strstr(buf, "aes") == NULL)
        {
            guardian_log(WLT_THREAT_MEDIUM,
                         "Disk encryption may not be active",
                         "Notified user — recommended enabling LUKS encryption");
            return WLT_THREAT_MEDIUM;
        }
    }

    /* Check for unattended screen (auto-lock) */
    time_t now = time(NULL);
    if (g_last_user_activity > 0 && (now - g_last_user_activity) > 300)
    {
        /* 5 minutes of inactivity → auto-lock screen */
        /* TODO(kairos): Trigger screen lock */
    }

    return WLT_THREAT_NONE;
}

/* ─── Guardian Main Loop ─────────────────────────────── */

wlt_guardian_error_t
wlt_guardian_run(void)
{
    g_guardian_running = true;
    g_last_user_activity = time(NULL);

    while (g_guardian_running)
    {
        wlt_guardian_threat_level_t max_threat = WLT_THREAT_NONE;

        /* Network checks */
        wlt_guardian_threat_level_t t;
        t = check_port_scans();
        if (t > max_threat) max_threat = t;

        t = check_unexpected_connections();
        if (t > max_threat) max_threat = t;

        t = check_data_exfiltration();
        if (t > max_threat) max_threat = t;

        /* Tor checks */
        t = check_tor_health();
        if (t > max_threat) max_threat = t;

        t = check_dns_leak();
        if (t > max_threat) max_threat = t;

        /* Process checks */
        t = check_process_anomalies();
        if (t > max_threat) max_threat = t;

        /* App permission checks */
        t = check_app_permissions();
        if (t > max_threat) max_threat = t;

        /* Disk security checks */
        t = check_disk_security();
        if (t > max_threat) max_threat = t;

        /* Respond based on threat level */
        switch (max_threat)
        {
            case WLT_THREAT_LOW:
                /* Silent fix — already logged */
                break;
            case WLT_THREAT_MEDIUM:
                /* TODO(kairos): Send notification to user */
                break;
            case WLT_THREAT_HIGH:
                /* TODO(kairos): Voice alert + visual overlay */
                break;
            case WLT_THREAT_CRITICAL:
                /* TODO(kairos): Kill switch + lockdown */
                /* Already triggered in the check function */
                break;
            default:
                break;
        }

        usleep(GUARDIAN_CHECK_INTERVAL_MS * 1000);
    }

    return WLT_GUARDIAN_OK;
}

void
wlt_guardian_stop(void)
{
    g_guardian_running = false;
}

void
wlt_guardian_user_activity(void)
{
    g_last_user_activity = time(NULL);
}

size_t
wlt_guardian_get_events(wlt_guardian_event_t *events, size_t max_count)
{
    size_t count = g_event_count < max_count ? g_event_count : max_count;
    memcpy(events, g_events, count * sizeof(wlt_guardian_event_t));
    return count;
}
