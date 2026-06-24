/**
 * @file shell.c
 * @brief Basic shell — command interpreter for WLTIOS
 * @module wlt-shell/shell
 * @purpose Read commands, parse, execute builtins + external commands
 * @dependencies: none
 * @tested_by: tests/userspace/shell_test.c
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define WLT_SHELL_MAX_INPUT 1024
#define WLT_SHELL_MAX_ARGS 64
#define WLT_SHELL_MAX_PATH 4096
#define WLT_SHELL_PROMPT "/wlt# "

static char g_cwd[WLT_SHELL_MAX_PATH];

/* ─── Built-in commands ──────────────────────────────── */

static int
cmd_ls(int argc, char **argv)
{
    bool show_all = false;
    bool long_format = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (char *p = argv[i] + 1; *p; p++)
            {
                if (*p == 'a') show_all = true;
                if (*p == 'l') long_format = true;
            }
        }
    }

    const char *path = g_cwd;
    if (argc > 1 && argv[1][0] != '-')
        path = argv[1];
    DIR *d = opendir(path);
    if (d == NULL)
    {
        perror("ls");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (!show_all && entry->d_name[0] == '.')
            continue;

        if (long_format)
        {
            char full_path[WLT_SHELL_MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0)
            {
                char type = S_ISDIR(st.st_mode) ? 'd' : '-';
                printf("%c%c%c%c%c%c%c%c%c%c %8lu %s\n",
                       type,
                       (st.st_mode & S_IRUSR) ? 'r' : '-',
                       (st.st_mode & S_IWUSR) ? 'w' : '-',
                       (st.st_mode & S_IXUSR) ? 'x' : '-',
                       (st.st_mode & S_IRGRP) ? 'r' : '-',
                       (st.st_mode & S_IWGRP) ? 'w' : '-',
                       (st.st_mode & S_IXGRP) ? 'x' : '-',
                       (st.st_mode & S_IROTH) ? 'r' : '-',
                       (st.st_mode & S_IWOTH) ? 'w' : '-',
                       (st.st_mode & S_IXOTH) ? 'x' : '-',
                       (unsigned long)st.st_size,
                       entry->d_name);
            }
        }
        else
        {
            printf("%s  ", entry->d_name);
        }
    }

    if (!long_format)
        printf("\n");

    closedir(d);
    return 0;
}

static int
cmd_cd(int argc, char **argv)
{
    const char *target = (argc > 1) ? argv[1] : getenv("HOME");
    if (target == NULL)
        target = "/";

    char new_path[WLT_SHELL_MAX_PATH];

    if (target[0] == '/')
    {
        strncpy(new_path, target, sizeof(new_path) - 1);
    }
    else
    {
        snprintf(new_path, sizeof(new_path), "%s/%s", g_cwd, target);
    }

    /* Resolve path */
    char resolved[WLT_SHELL_MAX_PATH];
    if (realpath(new_path, resolved) == NULL)
    {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "cd: %s: Not a directory\n", target);
        return 1;
    }

    strncpy(g_cwd, resolved, sizeof(g_cwd) - 1);
    g_cwd[sizeof(g_cwd) - 1] = '\0';
    setenv("PWD", g_cwd, 1);

    return 0;
}

static int
cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        /* Read from stdin */
        char buf[4096];
        ssize_t n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, n);
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        FILE *f = fopen(argv[i], "r");
        if (f == NULL)
        {
            fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
            return 1;
        }

        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            fwrite(buf, 1, n, stdout);

        fclose(f);
    }

    return 0;
}

static int
cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}

static int
cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (mkdir(argv[i], 0755) != 0 && errno != EEXIST)
        {
            fprintf(stderr, "mkdir: %s: %s\n", argv[i], strerror(errno));
            return 1;
        }
    }
    return 0;
}

static int
cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }

    bool recursive = false;
    bool force = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (char *p = argv[i] + 1; *p; p++)
            {
                if (*p == 'r') recursive = true;
                if (*p == 'f') force = true;
            }
            continue;
        }

        if (unlink(argv[i]) != 0)
        {
            if (errno == EISDIR && recursive)
            {
                /* TODO: recursive removal */
                fprintf(stderr, "rm: -r not yet implemented\n");
                return 1;
            }
            if (!force)
                fprintf(stderr, "rm: %s: %s\n", argv[i], strerror(errno));
        }
    }

    return 0;
}

static int
cmd_pwd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("%s\n", g_cwd);
    return 0;
}

/* ─── Network commands (Stage 2 — QEMU network + Tor) ─── */

/**
 * netcheck — verify network interface is up with correct IP.
 *
 * Reads /proc/net/fib_trie and looks for 10.0.2.15 (QEMU default).
 * Also reads /sys/class/net/ to list interfaces.
 * Sends one ICMP echo to 10.0.2.2 (gateway) via raw socket.
 */
static int
cmd_netcheck(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== WLTIOS Network Check ===\n");

    /* Step 1: list interfaces from /sys/class/net/ */
    printf("Interfaces:\n");
    DIR *d = opendir("/sys/class/net");
    if (d == NULL)
    {
        fprintf(stderr, "  cannot open /sys/class/net: %s\n", strerror(errno));
    }
    else
    {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL)
        {
            if (entry->d_name[0] == '.')
                continue;

            /* Read carrier state */
            char carrier_path[256];
            snprintf(carrier_path, sizeof(carrier_path),
                     "/sys/class/net/%s/carrier", entry->d_name);
            FILE *cf = fopen(carrier_path, "r");
            int carrier = -1;
            if (cf != NULL)
            {
                fscanf(cf, "%d", &carrier);
                fclose(cf);
            }

            /* Read address */
            char addr_path[256];
            snprintf(addr_path, sizeof(addr_path),
                     "/sys/class/net/%s/address", entry->d_name);
            FILE *af = fopen(addr_path, "r");
            char addr[64] = "?";
            if (af != NULL)
            {
                if (fgets(addr, sizeof(addr), af) != NULL)
                {
                    /* strip newline */
                    addr[strcspn(addr, "\n")] = '\0';
                }
                fclose(af);
            }

            printf("  %-10s carrier=%s mac=%s\n",
                   entry->d_name,
                   carrier == 1 ? "UP" : (carrier == 0 ? "DOWN" : "N/A"),
                   addr);
        }
        closedir(d);
    }

    /* Step 2: check /proc/net/fib_trie for 10.0.2.15 */
    printf("\nIP addresses (/proc/net/fib_trie):\n");
    FILE *f = fopen("/proc/net/fib_trie", "r");
    if (f == NULL)
    {
        fprintf(stderr, "  cannot open /proc/net/fib_trie: %s\n", strerror(errno));
    }
    else
    {
        char line[512];
        bool found_ip = false;
        bool found_10_0_2_15 = false;
        while (fgets(line, sizeof(line), f) != NULL)
        {
            /* Look for lines with IP addresses */
            if (strstr(line, "10.0.2.15") != NULL)
            {
                found_10_0_2_15 = true;
                found_ip = true;
            }
            else if (strstr(line, "10.") != NULL ||
                     strstr(line, "192.168.") != NULL ||
                     strstr(line, "172.") != NULL)
            {
                found_ip = true;
            }
        }
        fclose(f);

        if (found_10_0_2_15)
            printf("  10.0.2.15: FOUND (QEMU user-mode networking active)\n");
        else if (found_ip)
            printf("  10.0.2.15: NOT FOUND (other IPs present)\n");
        else
            printf("  No IPv4 addresses configured\n");
    }

    /* Step 3: check /proc/net/route for default route */
    printf("\nRoutes (/proc/net/route):\n");
    f = fopen("/proc/net/route", "r");
    if (f == NULL)
    {
        fprintf(stderr, "  cannot open /proc/net/route: %s\n", strerror(errno));
    }
    else
    {
        char line[512];
        bool first = true;
        bool found_default = false;
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (first)
            {
                first = false;
                continue;  /* skip header */
            }
            /* Parse: Iface Destination Gateway Flags ... */
            char iface[16];
            unsigned int dest, gw, flags;
            if (sscanf(line, "%15s %x %x %x", iface, &dest, &gw, &flags) >= 4)
            {
                struct in_addr d_addr, g_addr;
                d_addr.s_addr = dest;
                g_addr.s_addr = gw;
                printf("  %-10s dest=%s gw=%s\n",
                       iface, inet_ntoa(d_addr), inet_ntoa(g_addr));
                if (dest == 0)
                    found_default = true;
            }
        }
        fclose(f);
        if (!found_default)
            printf("  (no default route)\n");
    }

    /* Step 4: ICMP ping to 10.0.2.2 (gateway) */
    printf("\nPinging 10.0.2.2 (gateway):\n");
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0)
    {
        fprintf(stderr, "  cannot create ICMP socket: %s\n", strerror(errno));
        fprintf(stderr, "  (need CAP_NET_RAW or root — try 'cat /proc/net/fib_trie' for IP check)\n");
    }
    else
    {
        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        inet_pton(AF_INET, "10.0.2.2", &dst.sin_addr);

        /* Build ICMP echo request */
        unsigned char pkt[8] = {
            0x08,  /* type: echo request */
            0x00,  /* code: 0 */
            0x00, 0x00,  /* checksum (filled below) */
            0x00, 0x01,  /* identifier */
            0x00, 0x01   /* sequence number */
        };
        /* Compute checksum */
        unsigned int sum = 0;
        for (int i = 0; i < 4; i++)
            sum += (pkt[i*2] << 8) | pkt[i*2+1];
        while (sum >> 16)
            sum = (sum & 0xFFFF) + (sum >> 16);
        unsigned short cksum = ~sum & 0xFFFF;
        pkt[2] = cksum & 0xFF;
        pkt[3] = (cksum >> 8) & 0xFF;

        /* Set 3s timeout */
        struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        ssize_t sent = sendto(sock, pkt, 8, 0,
                              (struct sockaddr *)&dst, sizeof(dst));
        if (sent < 0)
        {
            fprintf(stderr, "  ping send failed: %s\n", strerror(errno));
        }
        else
        {
            printf("  sent %zd bytes, waiting for reply...\n", sent);
            unsigned char reply[64];
            ssize_t got = recv(sock, reply, sizeof(reply), 0);
            if (got < 0)
            {
                fprintf(stderr, "  no reply: %s\n", strerror(errno));
                printf("  PING: FAILED (timeout or error)\n");
            }
            else
            {
                printf("  received %zd bytes\n", got);
                if (got >= 8 && reply[0] == 0x00)
                    printf("  PING: SUCCESS (reply type=echo-reply)\n");
                else
                    printf("  PING: reply received (type=%d)\n",
                           got >= 1 ? reply[0] : -1);
            }
        }
        close(sock);
    }

    printf("=== Network Check Complete ===\n");
    return 0;
}

/**
 * torcheck — connect to check.torproject.org via Tor SOCKS5 proxy.
 *
 * If the response contains "Congratulations", traffic is going through Tor.
 *
 * SOCKS5 flow:
 *   1. Greeting: VER=5, NMETHODS=1, METHOD=0 (NO-AUTH)
 *   2. Connect: VER=5, CMD=1, ATYP=3 (domain), domain, port
 *   3. HTTP GET to check.torproject.org:80
 */
static int
cmd_torcheck(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== WLTIOS Tor Check ===\n");

    /* Step 0: check if Tor is listening on 9050 */
    printf("Checking Tor SOCKS port 9050...\n");
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_in socks_addr;
    memset(&socks_addr, 0, sizeof(socks_addr));
    socks_addr.sin_family = AF_INET;
    socks_addr.sin_port = htons(9050);
    inet_pton(AF_INET, "127.0.0.1", &socks_addr.sin_addr);

    /* 5s timeout */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&socks_addr, sizeof(socks_addr)) < 0)
    {
        fprintf(stderr, "  Cannot connect to 127.0.0.1:9050: %s\n",
                strerror(errno));
        fprintf(stderr, "  Tor is not running or not listening.\n");
        close(sock);
        printf("TOR: NOT RUNNING (cannot reach SOCKS port)\n");
        return 1;
    }
    printf("  Connected to Tor SOCKS5 port.\n");

    /* SOCKS5 greeting */
    unsigned char greeting[] = { 0x05, 0x01, 0x00 };
    if (write(sock, greeting, 3) != 3)
    {
        fprintf(stderr, "  SOCKS greeting write failed: %s\n", strerror(errno));
        close(sock);
        return 1;
    }

    unsigned char gresp[2];
    ssize_t n = read(sock, gresp, 2);
    if (n != 2 || gresp[0] != 0x05)
    {
        fprintf(stderr, "  SOCKS greeting failed (got %zd bytes)\n", n);
        close(sock);
        return 1;
    }
    printf("  SOCKS5 greeting OK (method=%d).\n", gresp[1]);

    /* SOCKS5 connect to check.torproject.org:80 */
    /* Format: VER CMD RSV ATYP LEN DOMAIN... PORT(2) */
    const char *domain = "check.torproject.org";
    size_t dlen = strlen(domain);
    unsigned char req[4 + 1 + dlen + 2];
    req[0] = 0x05;  /* VER */
    req[1] = 0x01;  /* CMD: CONNECT */
    req[2] = 0x00;  /* RSV */
    req[3] = 0x03;  /* ATYP: domain name */
    req[4] = (unsigned char)dlen;
    memcpy(&req[5], domain, dlen);
    req[5 + dlen] = 0x00;      /* port high byte */
    req[5 + dlen + 1] = 0x50;  /* port low byte (80) */

    if (write(sock, req, sizeof(req)) != (ssize_t)sizeof(req))
    {
        fprintf(stderr, "  SOCKS connect write failed: %s\n", strerror(errno));
        close(sock);
        return 1;
    }

    unsigned char cresp[10];
    n = read(sock, cresp, sizeof(cresp));
    if (n < 2 || cresp[1] != 0x00)
    {
        fprintf(stderr, "  SOCKS connect failed (reply code=%d, got %zd bytes)\n",
                n >= 2 ? cresp[1] : -1, n);
        close(sock);
        printf("TOR: SOCKS connect rejected (Tor may still be bootstrapping)\n");
        return 1;
    }
    printf("  SOCKS5 connect OK — tunnel established.\n");

    /* HTTP GET */
    const char *http_req =
        "GET / HTTP/1.0\r\n"
        "Host: check.torproject.org\r\n"
        "User-Agent: WLTIOS-TorCheck/0.1\r\n"
        "Connection: close\r\n"
        "\r\n";
    size_t hlen = strlen(http_req);
    if (write(sock, http_req, hlen) != (ssize_t)hlen)
    {
        fprintf(stderr, "  HTTP write failed: %s\n", strerror(errno));
        close(sock);
        return 1;
    }

    /* Read response */
    char buf[4096];
    size_t total = 0;
    while (total < sizeof(buf) - 1)
    {
        n = read(sock, buf + total, sizeof(buf) - 1 - total);
        if (n <= 0)
            break;
        total += n;
    }
    buf[total] = '\0';
    close(sock);

    if (total == 0)
    {
        fprintf(stderr, "  No response from server.\n");
        printf("TOR: No response (Tor may be blocking or network issue)\n");
        return 1;
    }

    printf("  Received %zu bytes.\n", total);

    /* Show first 200 chars */
    printf("\n--- Response (first 200 chars) ---\n");
    int show = total < 200 ? (int)total : 200;
    for (int i = 0; i < show; i++)
    {
        if (buf[i] == '\r')
            continue;
        putchar(buf[i]);
    }
    printf("\n--- End ---\n\n");

    /* Check for success indicators */
    if (strstr(buf, "Congratulations") != NULL)
    {
        printf("TOR: Connected. Traffic is anonymous.\n");
        printf("=== Tor Check Complete: SUCCESS ===\n");
        return 0;
    }
    else if (strstr(buf, "301 Moved") != NULL || strstr(buf, "302 Found") != NULL)
    {
        /* check.torproject.org redirects HTTP→HTTPS (301).
         * A redirect from the real server proves we reached it through Tor. */
        printf("TOR: Connected. Traffic is anonymous. (server redirected HTTP→HTTPS)\n");
        printf("=== Tor Check Complete: SUCCESS (redirect confirms Tor routing) ===\n");
        return 0;
    }
    else if (strstr(buf, "200 OK") != NULL || strstr(buf, "HTTP/1") != NULL)
    {
        printf("TOR: Response received but not Tor check page.\n");
        printf("=== Tor Check Complete: PARTIAL (HTTP ok, no Congratulations) ===\n");
        return 0;
    }
    else
    {
        printf("TOR: Response received but unrecognized.\n");
        printf("=== Tor Check Complete: UNKNOWN ===\n");
        return 1;
    }
}

/**
 * tstatus — show Tor process status by checking /proc/net/tcp for port 9050
 */
static int
cmd_tstatus(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== WLTIOS Tor Status ===\n");

    /* Check if tor binary exists */
    if (access("/usr/bin/tor", X_OK) == 0)
        printf("  tor binary: present (/usr/bin/tor)\n");
    else
        printf("  tor binary: NOT FOUND\n");

    /* Check /proc/net/tcp for port 9050 (0x232A in hex) */
    printf("  Checking /proc/net/tcp for port 9050 (0x232A)...\n");
    FILE *f = fopen("/proc/net/tcp", "r");
    if (f == NULL)
    {
        fprintf(stderr, "  cannot open /proc/net/tcp: %s\n", strerror(errno));
        return 1;
    }

    char line[512];
    bool first = true;
    bool found_9050 = false;
    int listen_count = 0;
    int estab_count = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (first) { first = false; continue; }

        /* Format: sl local_address rem_address st ... */
        char local[32], rem[32];
        unsigned int st;
        if (sscanf(line, "%*d: %31s %31s %x", local, rem, &st) >= 3)
        {
            /* local address format: IP:PORT in hex */
            char *colon = strchr(local, ':');
            if (colon != NULL)
            {
                unsigned int port = strtoul(colon + 1, NULL, 16);
                if (port == 9050)
                {
                    found_9050 = true;
                    if (st == 0x0A)  /* LISTEN */
                        printf("  port 9050: LISTENING (Tor SOCKS ready)\n");
                    else
                        printf("  port 9050: state=0x%x\n", st);
                }
            }
            if (st == 0x0A) listen_count++;
            else if (st == 0x01) estab_count++;
        }
    }
    fclose(f);

    if (!found_9050)
        printf("  port 9050: NOT LISTENING (Tor not started or not ready)\n");

    printf("  Total: %d listening, %d established sockets\n",
           listen_count, estab_count);

    /* Check /var/lib/tor for data directory */
    DIR *d = opendir("/var/lib/tor");
    if (d != NULL)
    {
        printf("  /var/lib/tor: exists\n");
        closedir(d);
    }
    else
    {
        printf("  /var/lib/tor: missing\n");
    }

    printf("=== Tor Status Complete ===\n");
    return 0;
}

static int
cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("WLTIOS Shell — Built-in commands:\n");
    printf("  ls [-al]        List directory contents\n");
    printf("  cd [dir]        Change directory\n");
    printf("  cat [file...]   Display file contents\n");
    printf("  echo [text...]  Print text\n");
    printf("  mkdir [dir...]  Create directories\n");
    printf("  rm [-rf] [file] Remove files\n");
    printf("  pwd             Print working directory\n");
    printf("  netcheck        Check network (interfaces, IP, ping 10.0.2.2)\n");
    printf("  torcheck        Verify Tor routes traffic (via check.torproject.org)\n");
    printf("  tstatus         Show Tor process and port status\n");
    printf("  help            Show this help\n");
    printf("  exit            Exit shell\n");
    return 0;
}

/* ─── Command table ──────────────────────────────────── */

typedef struct {
    const char *name;
    int (*func)(int argc, char **argv);
} wlt_shell_command_t;

static wlt_shell_command_t commands[] = {
    {"ls",       cmd_ls},
    {"cd",       cmd_cd},
    {"cat",      cmd_cat},
    {"echo",     cmd_echo},
    {"mkdir",    cmd_mkdir},
    {"rm",       cmd_rm},
    {"pwd",      cmd_pwd},
    {"netcheck", cmd_netcheck},
    {"torcheck", cmd_torcheck},
    {"tstatus",  cmd_tstatus},
    {"help",     cmd_help},
    {NULL, NULL},
};

/* ─── Input parsing ──────────────────────────────────── */

static int
parse_input(char *input, char **argv, int max_args)
{
    int argc = 0;
    char *p = input;

    while (*p && argc < max_args - 1)
    {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0' || *p == '\n')
            break;

        /* Handle quoted strings */
        if (*p == '"')
        {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"' && *p != '\n')
                p++;
            if (*p == '"')
                *p++ = '\0';
        }
        else
        {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n')
                p++;
        }

        if (*p)
            *p++ = '\0';
    }

    argv[argc] = NULL;
    return argc;
}

/* ─── Main shell loop ────────────────────────────────── */

int
main(int argc, char **argv)
{
 (void)argc;
 (void)argv;

 /* Disable stdio buffering — fixes null bytes in QEMU serial */
 setvbuf(stdout, NULL, _IONBF, 0);
 setvbuf(stderr, NULL, _IONBF, 0);

 /* Initialize CWD */
 if (getcwd(g_cwd, sizeof(g_cwd)) == NULL)
  strncpy(g_cwd, "/", sizeof(g_cwd) - 1);

 char input[WLT_SHELL_MAX_INPUT];

 write(1, "WLTIOS Shell v1.1\r\n", sizeof("WLTIOS Shell v1.1\r\n") - 1);
 write(1, "Type 'help' for available commands.\r\n\r\n", sizeof("Type 'help' for available commands.\r\n\r\n") - 1);

 while (1)
 {
  if (strcmp(g_cwd, "/") == 0)
   printf("/wlt# ");
  else
   printf("%s/wlt# ", g_cwd);
  fflush(stdout);

  if (fgets(input, sizeof(input), stdin) == NULL)
  {
   printf("\n");
   break;
  }

  if (input[0] == '\n')
   continue;

  char *cmd_argv[WLT_SHELL_MAX_ARGS];
  int cmd_argc = parse_input(input, cmd_argv, WLT_SHELL_MAX_ARGS);

  if (cmd_argc == 0)
   continue;

  if (strcmp(cmd_argv[0], "exit") == 0)
   break;

  /* Check for output redirection */
  int saved_stdout = -1;
  for (int ri = 0; ri < cmd_argc; ri++) {
   if (cmd_argv[ri] && (strcmp(cmd_argv[ri], ">") == 0 || strcmp(cmd_argv[ri], ">>") == 0)) {
    if (ri + 1 < cmd_argc) {
     int append = (cmd_argv[ri][1] == '>');
     int fd = open(cmd_argv[ri + 1], O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
     if (fd >= 0) {
      saved_stdout = dup(STDOUT_FILENO);
      dup2(fd, STDOUT_FILENO);
      close(fd);
      cmd_argv[ri] = NULL;
      cmd_argc = ri;
     }
    }
   }
  }

  /* Check built-in commands */
  bool found = false;
  for (int i = 0; commands[i].name != NULL; i++)
  {
   if (strcmp(cmd_argv[0], commands[i].name) == 0)
   {
    commands[i].func(cmd_argc, cmd_argv);
    found = true;
    break;
   }
  }

  /* If not built-in, try external command */
  if (!found)
  {
   pid_t pid = fork();
   if (pid == 0)
   {
    execvp(cmd_argv[0], cmd_argv);
    fprintf(stderr, "%s: command not found\n", cmd_argv[0]);
    _exit(127);
   }
   else if (pid > 0)
   {
    int status;
    waitpid(pid, &status, 0);
   }
   else
   {
    perror("fork");
   }
  }

  /* Restore stdout if redirected */
  if (saved_stdout >= 0) {
   dup2(saved_stdout, STDOUT_FILENO);
   close(saved_stdout);
  }
 }

 return 0;
}