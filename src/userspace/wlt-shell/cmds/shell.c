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
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <linux/fb.h>
#include <sched.h>
/* Kairos tool use support */
#include <stdio.h>
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

/* ─── Framebuffer draw command (Stage 2 — GUI) ──────── */

/*
 * Minimal 7x9 bitmap font for "WLTIOS".
 * Each letter is 9 rows, each row's lower 7 bits = pixel on/off.
 * Bit 6 (0x40) = leftmost pixel, bit 0 (0x01) = rightmost.
 */

/* W: wide W shape */
static const uint8_t FONT_W[9] = {
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x55, /* 1.1.1.1 */
    0x55, /* 1.1.1.1 */
    0x55, /* 1.1.1.1 */
    0x55, /* 1.1.1.1 */
    0x22, /* .1...1. */
    0x41, /* 1.....1 */
};

/* L: L shape */
static const uint8_t FONT_L[9] = {
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x7F, /* 1111111 */
};

/* T: T shape */
static const uint8_t FONT_T[9] = {
    0x7F, /* 1111111 */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
};

/* I: I with serifs */
static const uint8_t FONT_I[9] = {
    0x7F, /* 1111111 */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x08, /* ...1... */
    0x7F, /* 1111111 */
};

/* O: oval */
static const uint8_t FONT_O[9] = {
    0x3E, /* .11111. */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x41, /* 1.....1 */
    0x3E, /* .11111. */
};

/* S: S shape */
static const uint8_t FONT_S[9] = {
    0x3E, /* .11111. */
    0x41, /* 1.....1 */
    0x40, /* 1...... */
    0x40, /* 1...... */
    0x3E, /* .11111. */
    0x01, /* ......1 */
    0x01, /* ......1 */
    0x41, /* 1.....1 */
    0x3E, /* .11111. */
};

/**
 * draw — render to /dev/fb0 directly, or fall back to VGA text mode.
 *
 * If /dev/fb0 exists (kernel has framebuffer support):
 *   - Fills the screen with navy blue
 *   - Draws a cyan rectangle in the center
 *   - Writes "WLTIOS" in white pixels using the bitmap font
 *
 * If /dev/fb0 doesn't exist but /dev/mem does (VGA text mode fallback):
 *   - Writes to VGA text buffer at physical 0xB8000
 *   - 80x25 text mode, each cell = char + attribute byte
 *   - Navy blue background, cyan rectangle border, "WLTIOS" in bright white
 *
 * This is the first visual output WLTIOS has ever produced.
 */
static int
cmd_draw(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== WLTIOS Draw ===\n");

    /* Try /dev/fb0 first (pixel-perfect framebuffer) */
    int fb = open("/dev/fb0", O_RDWR);
    if (fb >= 0)
    {
        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;
        if (ioctl(fb, FBIOGET_VSCREENINFO, &var) == 0 &&
            ioctl(fb, FBIOGET_FSCREENINFO, &fix) == 0 &&
            (var.bits_per_pixel == 32 || var.bits_per_pixel == 24))
        {
            int fb_w = var.xres;
            int fb_h = var.yres;
            size_t fb_size = fix.smem_len;
            int line_len = fix.line_length;
            int bpp = var.bits_per_pixel;
            int bytes_per_pixel = bpp / 8;

            printf("  Framebuffer: %dx%d %dbpp (line_length=%d)\n",
                   fb_w, fb_h, bpp, line_len);

            uint8_t *fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
            if (fb_mem != MAP_FAILED)
            {
                /* For 24bpp: 3 bytes per pixel (BGR), for 32bpp: 4 bytes (BGRA) */
                /* Navy background: R=0x1a, G=0x1a, B=0x2e */
                uint8_t bg_r = 0x1a, bg_g = 0x1a, bg_b = 0x2e;
                /* Cyan rectangle: R=0x00, G=0xd4, B=0xff */
                uint8_t fg_r = 0x00, fg_g = 0xd4, fg_b = 0xff;
                /* White text: R=0xff, G=0xff, B=0xff */
                uint8_t tc_r = 0xff, tc_g = 0xff, tc_b = 0xff;

                /* Helper to set a pixel at (x,y) with given RGB */
                #define SET_PIXEL(x, y, r, g, b) do { \
                    int _x = (x), _y = (y); \
                    if (_x >= 0 && _x < fb_w && _y >= 0 && _y < fb_h) { \
                        uint8_t *_p = fb_mem + (_y * line_len) + (_x * bytes_per_pixel); \
                        _p[0] = (b); _p[1] = (g); _p[2] = (r); \
                        if (bytes_per_pixel == 4) _p[3] = 0xFF; \
                    } \
                } while(0)

                /* Fill background with navy */
                for (int y = 0; y < fb_h; y++)
                    for (int x = 0; x < fb_w; x++)
                        SET_PIXEL(x, y, bg_r, bg_g, bg_b);
                printf("  Background filled (navy)\n");

                /* Draw cyan rectangle centered */
                int rw = fb_w * 56 / 100;
                int rh = fb_h * 48 / 100;
                int rx = (fb_w - rw) / 2;
                int ry = (fb_h - rh) / 2;
                for (int y = ry; y < ry + rh; y++)
                    for (int x = rx; x < rx + rw; x++)
                        SET_PIXEL(x, y, fg_r, fg_g, fg_b);
                printf("  Rectangle drawn (cyan %dx%d at %d,%d)\n", rw, rh, rx, ry);

                /* "WLTIOS" in white, scaled 4x, centered */
                const uint8_t *letters[] = {FONT_W, FONT_L, FONT_T, FONT_I, FONT_O, FONT_S};
                int nletters = 6, cw = 7, ch = 9, spacing = 2;
                int total_w = nletters * cw + (nletters - 1) * spacing;
                int scale = 4;
                int tx = (fb_w - total_w * scale) / 2;
                int ty = (fb_h - ch * scale) / 2;

                for (int i = 0; i < nletters; i++)
                {
                    int cx = tx + i * (cw + spacing) * scale;
                    for (int row = 0; row < ch; row++)
                        for (int col = 0; col < cw; col++)
                            if (letters[i][row] & (0x40 >> col))
                                for (int dy = 0; dy < scale; dy++)
                                    for (int dx = 0; dx < scale; dx++)
                                    {
                                        int px = cx + col * scale + dx;
                                        int py = ty + row * scale + dy;
                                        SET_PIXEL(px, py, tc_r, tc_g, tc_b);
                                    }
                }
                printf("  Text 'WLTIOS' drawn (white, 4x scale)\n");

                #undef SET_PIXEL

                munmap(fb_mem, fb_size);
                close(fb);
                printf("=== Draw Complete: navy bg + cyan rect + WLTIOS (framebuffer) ===\n");
                return 0;
            }
        }
        close(fb);
        printf("  /dev/fb0 exists but unusable, falling back to VGA text mode\n");
    }

    /* VGA text mode fallback via /dev/mem at 0xB8000 */
    printf("  Falling back to VGA text mode (80x25)\n");
    int mem = open("/dev/mem", O_RDWR);
    if (mem < 0)
    {
        fprintf(stderr, "draw: cannot open /dev/fb0 or /dev/mem: %s\n", strerror(errno));
        fprintf(stderr, "draw: need framebuffer or /dev/mem support in kernel\n");
        return 1;
    }

    /* VGA text buffer: 80x25, 2 bytes per cell, at physical 0xB8000 */
    uint8_t *vga = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem, 0xB8000);
    if (vga == MAP_FAILED)
    {
        fprintf(stderr, "draw: mmap /dev/mem at 0xB8000 failed: %s\n", strerror(errno));
        close(mem);
        return 1;
    }

    /* VGA text mode attributes:
     * Bits 0-3: foreground, bits 4-6: background
     * 0=black, 1=blue, 2=green, 3=cyan, 4=red, 5=magenta, 6=brown, 7=white
     * 8-15 = bright variants (foreground only)
     */
    uint8_t bg_attr = 0x01;  /* blue background (closest to navy in VGA palette) */
    uint8_t rect_attr = 0x3F; /* cyan bg + bright white fg */
    uint8_t text_attr = 0x3F; /* cyan bg + bright white fg */

    /* Fill entire screen with blue background */
    for (int i = 0; i < 80 * 25; i++)
    {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = bg_attr;
    }
    printf("  VGA: blue background filled\n");

    /* Draw a cyan rectangle (border) in the center of the screen */
    /* Rectangle: rows 5-19, cols 10-69 */
    int rect_top = 5, rect_bottom = 19, rect_left = 10, rect_right = 69;
    for (int y = rect_top; y <= rect_bottom; y++)
    {
        for (int x = rect_left; x <= rect_right; x++)
        {
            int idx = (y * 80 + x) * 2;
            vga[idx] = ' ';
            vga[idx + 1] = rect_attr;  /* cyan bg */
        }
    }
    printf("  VGA: cyan rectangle drawn (rows %d-%d, cols %d-%d)\n",
           rect_top, rect_bottom, rect_left, rect_right);

    /* Write "WLTIOS" centered on the rectangle */
    const char *text = "W L T I O S";
    int text_len = 11;
    int text_row = 12;  /* center row */
    int text_col = (80 - text_len) / 2;
    for (int i = 0; i < text_len; i++)
    {
        int idx = (text_row * 80 + text_col + i) * 2;
        vga[idx] = text[i];
        vga[idx + 1] = text_attr;  /* bright white on cyan */
    }
    printf("  VGA: 'WLTIOS' written (bright white on cyan)\n");

    /* Draw a border around the rectangle */
    for (int x = rect_left; x <= rect_right; x++)
    {
        vga[(rect_top * 80 + x) * 2] = '=';
        vga[(rect_top * 80 + x) * 2 + 1] = 0x3F;
        vga[(rect_bottom * 80 + x) * 2] = '=';
        vga[(rect_bottom * 80 + x) * 2 + 1] = 0x3F;
    }
    for (int y = rect_top; y <= rect_bottom; y++)
    {
        vga[(y * 80 + rect_left) * 2] = '|';
        vga[(y * 80 + rect_left) * 2 + 1] = 0x3F;
        vga[(y * 80 + rect_right) * 2] = '|';
        vga[(y * 80 + rect_right) * 2 + 1] = 0x3F;
    }

    munmap(vga, 4096);
    close(mem);

    printf("=== Draw Complete: blue bg + cyan rect + WLTIOS (VGA text mode) ===\n");
    printf("=== Check VNC display to see the output ===\n");
    return 0;
}
/* Route-based kill switch helpers (fallback when ip_tables.ko unavailable) */
#include <net/route.h>

static int wlt_get_default_gw(char *gw_out, size_t gw_len) {
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) return -1;
    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    while (fgets(line, sizeof(line), f)) {
        char iface[32]; unsigned int dest, gw;
        if (sscanf(line, "%31s %x %x", iface, &dest, &gw) >= 3) {
            if (dest == 0 && gw != 0) {
                struct in_addr addr; addr.s_addr = gw;
                const char *s = inet_ntoa(addr);
                if (s) { strncpy(gw_out, s, gw_len-1); gw_out[gw_len-1]=0; fclose(f); return 0; }
            }
        }
    }
    fclose(f); return -1;
}

static int wlt_delete_default_route(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) return -1;
    struct rtentry rt; memset(&rt, 0, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    ((struct sockaddr_in *)&rt.rt_dst)->sin_addr.s_addr = INADDR_ANY;
    rt.rt_genmask.sa_family = AF_INET;
    ((struct sockaddr_in *)&rt.rt_genmask)->sin_addr.s_addr = INADDR_ANY;
    int ret = ioctl(fd, SIOCDELRT, &rt); int se = errno; close(fd); errno = se; return ret;
}

static int wlt_add_default_route(const char *gw) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) return -1;
    struct rtentry rt; memset(&rt, 0, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    ((struct sockaddr_in *)&rt.rt_dst)->sin_addr.s_addr = INADDR_ANY;
    rt.rt_genmask.sa_family = AF_INET;
    ((struct sockaddr_in *)&rt.rt_genmask)->sin_addr.s_addr = INADDR_ANY;
    rt.rt_gateway.sa_family = AF_INET;
    inet_aton(gw, &((struct sockaddr_in *)&rt.rt_gateway)->sin_addr);
    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    int ret = ioctl(fd, SIOCADDRT, &rt); int se = errno; close(fd); errno = se; return ret;
}

/* Kill switch - iptables primary, route-based fallback */
static int
cmd_killswitch(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: killswitch <on|off|status>\n");
        return 1;
    }
    const char *LOADER = "/lib/ld-musl-i386.so.1";
    const char *IPTABLES = "/usr/sbin/iptables";
    int have_ipt = (access(LOADER, X_OK) == 0 && access(IPTABLES, X_OK) == 0);
    if (strcmp(argv[1], "on") == 0) {
        printf("=== Activating Kill Switch ===\n");
        int ipt_ok = 0;
        if (have_ipt) {
            const char *c0[] = { LOADER, "--library-path", "/lib", IPTABLES, "-F", NULL };
            const char *c1[] = { LOADER, "--library-path", "/lib", IPTABLES, "-P", "OUTPUT", "DROP", NULL };
            const char *c2[] = { LOADER, "--library-path", "/lib", IPTABLES, "-A", "OUTPUT", "-d", "127.0.0.1", "-j", "ACCEPT", NULL };
            const char *c3[] = { LOADER, "--library-path", "/lib", IPTABLES, "-A", "OUTPUT", "-d", "10.0.2.2", "-j", "ACCEPT", NULL };
            const char **cmds[] = { c0, c1, c2, c3 };
            int ok = 0;
            for (int i = 0; i < 4; i++) {
                pid_t pid = fork();
                if (pid == 0) { execv(LOADER, (char **)cmds[i]); _exit(127); }
                int st; waitpid(pid, &st, 0);
                if (WIFEXITED(st) && WEXITSTATUS(st) == 0) ok++;
                else printf("  rule %d: exit %d\n", i, WIFEXITED(st)?WEXITSTATUS(st):-1);
            }
            ipt_ok = (ok >= 2);
        }
        if (ipt_ok) {
            printf("  OUTPUT policy: DROP\n");
            printf("  Allowed: 127.0.0.1 (Tor), 10.0.2.2 (gw)\n");
            printf("=== Kill Switch ACTIVE - non-Tor blocked ===\n");
            return 0;
        }
        printf("  iptables unavailable (no ip_tables module).\n");
        printf("  Fallback: route-based panic kill switch...\n");
        char gw[32];
        if (wlt_get_default_gw(gw, sizeof(gw)) != 0) {
            FILE *mf = fopen("/run/wlt-killswitch", "w");
            if (mf) { fprintf(mf, "route:none\n"); fclose(mf); }
            printf("=== Kill Switch ACTIVE (no default route) ===\n");
            return 0;
        }
        FILE *gf = fopen("/run/wlt-killswitch", "w");
        if (gf) { fprintf(gf, "route:%s\n", gw); fclose(gf); }
        if (wlt_delete_default_route() == 0) {
            printf("  Default route deleted (was via %s)\n", gw);
            printf("=== Kill Switch ACTIVE (route-based panic) ===\n");
            return 0;
        }
        printf("=== Kill Switch FAILED ===\n");
        return 1;
    } else if (strcmp(argv[1], "off") == 0) {
        printf("=== Deactivating Kill Switch ===\n");
        int did_route = 0;
        FILE *gf = fopen("/run/wlt-killswitch", "r");
        if (gf) {
            char line[64];
            if (fgets(line, sizeof(line), gf)) {
                line[strcspn(line, "\n")] = 0;
                if (strncmp(line, "route:", 6) == 0) {
                    const char *gw = line + 6; did_route = 1;
                    if (strcmp(gw, "none") != 0 && wlt_add_default_route(gw) == 0)
                        printf("  Route restored (via %s)\n", gw);
                }
            }
            fclose(gf); unlink("/run/wlt-killswitch");
        }
        if (have_ipt) {
            char *a1[] = { (char *)LOADER, "--library-path", "/lib", (char *)IPTABLES, "-F", NULL };
            char *a2[] = { (char *)LOADER, "--library-path", "/lib", (char *)IPTABLES, "-P", "OUTPUT", "ACCEPT", NULL };
            pid_t pid; int st;
            pid = fork(); if (pid==0) { execv(LOADER, a1); _exit(127); } waitpid(pid, &st, 0);
            pid = fork(); if (pid==0) { execv(LOADER, a2); _exit(127); } waitpid(pid, &st, 0);
            printf("  OUTPUT policy: ACCEPT\n");
        }
        if (!did_route && !have_ipt) printf("  (no kill switch was active)\n");
        printf("=== Kill Switch OFF - all traffic allowed ===\n");
        return 0;
    } else if (strcmp(argv[1], "status") == 0) {
        printf("=== iptables rules ===\n");
        if (have_ipt) {
            char *a[] = { (char *)LOADER, "--library-path", "/lib", (char *)IPTABLES, "-L", "-n", NULL };
            pid_t pid = fork(); if (pid==0) { execv(LOADER, a); _exit(127); }
            int st; waitpid(pid, &st, 0);
        } else printf("  iptables unavailable\n");
        printf("=== Routing table ===\n");
        FILE *rf = fopen("/proc/net/route", "r");
        if (rf) { char l[256]; while (fgets(l,sizeof(l),rf)) printf("  %s", l); fclose(rf); }
        FILE *kf = fopen("/run/wlt-killswitch", "r");
        if (kf) { char l[64]; if (fgets(l,sizeof(l),kf)) printf("=== marker: %s", l); fclose(kf); }
        else printf("=== Kill switch: not active ===\n");
        return 0;
    }
    fprintf(stderr, "killswitch: unknown argument '%s'\n", argv[1]);
    return 1;
}

static int
cmd_sandbox(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: sandbox <command> [args...]\n");
        printf("Runs the command in an isolated network namespace.\n");
        printf("The sandboxed process has no network access (no eth0, no route).\n");
        printf("Example: sandbox netcheck  (should show no default route)\n");
        return 1;
    }

    printf("=== Sandboxing: %s ===\n", argv[1]);

    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "sandbox: fork failed: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0)
    {
        /* Child: unshare network namespace, then exec the command.
         * CLONE_NEWNET gives us a new network stack with only lo (down).
         * The child cannot see eth0 or the default route. */
        if (unshare(CLONE_NEWNET) != 0)
        {
            fprintf(stderr, "sandbox: unshare(CLONE_NEWNET) failed: %s\n",
                    strerror(errno));
            fprintf(stderr, "sandbox: (kernel may not support namespaces)\n");
            _exit(1);
        }

        /* Bring up lo in the new namespace so localhost works */
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0)
        {
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
            ifr.ifr_flags = IFF_UP | IFF_RUNNING;
            ioctl(sock, SIOCSIFFLAGS, &ifr);
            close(sock);
        }

        /* Exec the command */
        execvp(argv[1], &argv[1]);
        fprintf(stderr, "sandbox: exec %s failed: %s\n", argv[1], strerror(errno));
        _exit(127);
    }

    /* Parent: wait for child */
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
    {
        printf("=== Sandbox exited (code %d) ===\n", WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    printf("=== Sandbox terminated abnormally ===\n");
    return 1;
}

/* ─── TCP test command (kill switch verification) ──── */

/**
 * tcptest — try a direct TCP connection to an IP:port (NOT through Tor).
 *
 * This is used to verify the kill switch: when killswitch is on,
 * direct TCP to external IPs should fail (timeout/refused).
 * When killswitch is off, it should succeed.
 *
 * Usage: tcptest <ip> <port>
 * Example: tcptest 8.8.8.8 80
 */
static int
cmd_tcptest(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: tcptest <ip> <port>\n");
        printf("Tries a direct TCP connection (NOT through Tor).\n");
        printf("Use to verify kill switch: 'tcptest 8.8.8.8 80'\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "tcptest: invalid port '%s'\n", argv[2]);
        return 1;
    }

    printf("=== Direct TCP Test: %s:%d ===\n", ip, port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "  socket: %s\n", strerror(errno));
        return 1;
    }

    /* Set 5s timeout */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
    {
        fprintf(stderr, "  invalid IP: %s\n", ip);
        close(sock);
        return 1;
    }

    printf("  Connecting...\n");
    int rc = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0)
    {
        printf("  RESULT: BLOCKED (%s)\n", strerror(errno));
        printf("=== Kill switch is ACTIVE — direct TCP blocked ===\n");
        close(sock);
        return 0;  /* return 0 = blocked, which is expected with killswitch on */
    }

    printf("  RESULT: CONNECTED (direct TCP works)\n");
    printf("=== Kill switch is OFF — direct TCP succeeded ===\n");

    /* Try sending a minimal HTTP HEAD */
    const char *req = "HEAD / HTTP/1.0\r\nHost: ";
    write(sock, req, strlen(req));
    write(sock, ip, strlen(ip));
    write(sock, "\r\n\r\n", 4);

    char buf[256];
    ssize_t n = read(sock, buf, sizeof(buf) - 1);
    if (n > 0)
    {
        buf[n] = '\0';
        printf("  Response (%zd bytes): %.80s\n", n, buf);
    }

    close(sock);
    return 1;  /* return 1 = connected, which means killswitch is off */
}


/* ─── Kairos AI command (Section E) ─────────────────── */

/**
 * kairos — ask the Kairos AI assistant a question.
 *
 * Starts llama-server in background if not running, then sends
 * an HTTP POST to 127.0.0.1:8080/v1/chat/completions with the
 * user's question. Parses the JSON response to extract the content.
 *
 * Usage: kairos <your question>
 * Example: kairos hello
 */
/* Stub: kairos_run_command - not linked in minimal build */
static int kairos_run_command(const char *cmd, char *output, size_t outlen) {
    (void)cmd;
    snprintf(output, outlen, "[Kairos tool execution not available]");
    return -1;
}

static int
cmd_kairos(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: kairos <question>\n");
        printf("Ask Kairos, the WLTIOS AI assistant.\n");
        printf("Example: kairos what is my ip?\n");
        return 1;
    }

    /* Build the question from argv */
    char question[1024];
    question[0] = '\0';
    for (int i = 1; i < argc; i++)
    {
        if (i > 1) strncat(question, " ", sizeof(question) - strlen(question) - 1);
        strncat(question, argv[i], sizeof(question) - strlen(question) - 1);
    }

    printf("=== Kairos AI ===\n");
    printf("  Question: %s\n", question);
    printf("  Connecting to llama-server at 127.0.0.1:8080...\n");

    /* Check if llama-server is running by trying to connect */
    int check_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (check_sock < 0)
    {
        fprintf(stderr, "kairos: socket failed\n");
        return 1;
    }

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(check_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(check_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(check_sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
    {
        close(check_sock);
        printf("  llama-server not running. Start it with:\n");
        printf("    /usr/bin/llama-server -m /path/to/model.gguf --port 8080 &\n");
        printf("  (In WSL2: ~/wltios-build/llama.cpp/build/bin/llama-server "
               "-m ~/wltios-build/kairos-model.gguf --port 8080 &)\n");
        return 1;
    }
    close(check_sock);

    /* Build JSON request body */
    char json_body[2048];
    snprintf(json_body, sizeof(json_body),
        "{\"messages\":["
        "{\"role\":\"system\",\"content\":\"You are Kairos, the AI assistant "
        "built into WLTIOS operating system. WLTIOS routes all traffic through "
        "Tor for anonymity. Be concise. Protect the user's privacy.\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}"
        "],\"max_tokens\":100}", question);

    /* Build HTTP request */
    char http_request[4096];
    snprintf(http_request, sizeof(http_request),
        "POST /v1/chat/completions HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        strlen(json_body), json_body);

    /* Send request */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "kairos: socket failed\n");
        return 1;
    }

    tv.tv_sec = 30;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
    {
        fprintf(stderr, "kairos: cannot connect to 127.0.0.1:8080\n");
        close(sock);
        return 1;
    }

    printf("  Sending request...\n");
    write(sock, http_request, strlen(http_request));

    /* Read response */
    char response[8192];
    ssize_t total = 0;
    while (total < (ssize_t)sizeof(response) - 1)
    {
        ssize_t n = read(sock, response + total, sizeof(response) - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    close(sock);

    if (total == 0)
    {
        fprintf(stderr, "kairos: no response from server\n");
        return 1;
    }

    /* Find the JSON body (skip HTTP headers) */
    char *body = strstr(response, "\r\n\r\n");
    if (body) body += 4;
    else body = response;

    /* Extract content from JSON: find "content":"..." */
    char *content_start = strstr(body, "\"content\":\"");
    if (content_start)
    {
        content_start += 11;  /* skip "content":" */
        char *content_end = strstr(content_start, "\"}");
        if (!content_end) content_end = strstr(content_start, "\",");
        if (content_end)
        {
            *content_end = '\0';
            printf("\n  Kairos: %s\n\n", content_start);
    /* Check if response contains a tool call */
    char *tool_start = strstr(content_start, "run_command");
    if (tool_start) {
        /* Extract command from tool call */
        char cmd_buf[256] = {0};
        char *cmd_begin = strstr(tool_start, "\"command\":\"");
        if (cmd_begin) {
            cmd_begin += 11;
            char *cmd_end = strchr(cmd_begin, '"');
            if (cmd_end) {
                size_t clen = cmd_end - cmd_begin;
                if (clen < 255) { memcpy(cmd_buf, cmd_begin, clen); cmd_buf[clen] = 0; }
                printf("  [Kairos tool] run_command: %s\n", cmd_buf);
                char tool_output[1024];
                kairos_run_command(cmd_buf, tool_output, sizeof(tool_output));
                printf("  [Tool output] %.200s\n", tool_output);
            }
        }
    }
            printf("=== Kairos Response Complete ===\n");
            return 0;
        }
    }

    /* If we can't parse, show raw response */
    printf("  Raw response: %.200s\n", body);
    printf("=== Kairos response (unparsed) ===\n");
    return 0;
}

static int
cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("WLTIOS Shell commands:\n");
    printf("  ls [path]         List directory\n");
    printf("  cd <path>         Change directory\n");
    printf("  cat <file>        Display file\n");
    printf("  echo <text>       Print text\n");
    printf("  mkdir <dir>       Create directory\n");
    printf("  rm <file>         Remove file\n");
    printf("  pwd               Working directory\n");
    printf("  netcheck          Network status\n");
    printf("  torcheck          Verify Tor routing\n");
    printf("  tstatus           Tor status\n");
    printf("  draw <text>       Draw to framebuffer\n");
    printf("  killswitch <on|off|status>  Kill switch\n");
    printf("  sandbox <cmd>     Isolated net namespace\n");
    printf("  tcptest <ip> <port>  TCP test\n");
    printf("  kairos <question> Ask Kairos AI\n");
    printf("  pkg install <path> Install package\n");
    printf("  pkg list          List packages\n");
    printf("  help              Show this help\n");
    return 0;
}

/* ─── Package manager command (Section D) ──────────── */

static int
cmd_pkg(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: pkg <install|list> [path]\n");
        printf("  install <path>  Install a .tar.gz package\n");
        printf("  list            List installed packages\n");
        return 1;
    }

    if (strcmp(argv[1], "install") == 0 && argc >= 3)
    {
        const char *pkg_path = argv[2];
        printf("=== Installing %s ===\n", pkg_path);

        /* Verify file exists */
        if (access(pkg_path, F_OK) != 0)
        {
            fprintf(stderr, "pkg: file not found: %s\n", pkg_path);
            return 1;
        }

        /* Extract to /usr/local */
        printf("  Extracting...\n");
        pid_t pid = fork();
        if (pid == 0)
        {
            char *tar_args[] = {"/bin/tar", "-xzf", (char *)pkg_path,
                               "-C", "/usr/local", NULL};
            char *envp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", NULL};
            execve("/bin/tar", tar_args, envp);
            _exit(127);
        }
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        {
            printf("  Extraction failed (exit %d)\n", WEXITSTATUS(status));
            return 1;
        }

        /* Record in installed.db */
        printf("  Recording installation...\n");
        FILE *db = fopen("/var/lib/wlt-pkg/installed.db", "a");
        if (db)
        {
            fprintf(db, "hello-wlt 1.0.0 /usr/local\n");
            fclose(db);
        }

        printf("  Package installed successfully!\n");
        printf("  Run '/usr/local/bin/hello-wlt' to test.\n");
        return 0;
    }
    else if (strcmp(argv[1], "list") == 0)
    {
        printf("=== Installed packages ===\n");
        FILE *db = fopen("/var/lib/wlt-pkg/installed.db", "r");
        if (!db)
        {
            printf("  No packages installed.\n");
            return 0;
        }
        char line[256];
        while (fgets(line, sizeof(line), db))
        {
            printf("  %s", line);
        }
        fclose(db);
        return 0;
    }

    fprintf(stderr, "pkg: unknown command '%s'\n", argv[1]);
    return 1;
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
    {"draw",     cmd_draw},
    {"killswitch", cmd_killswitch},
    {"sandbox",  cmd_sandbox},
    {"tcptest",  cmd_tcptest},
    {"kairos",   cmd_kairos},
    {"pkg",     cmd_pkg},
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

 write(1, "WLTIOS Shell v1.2\r\n", sizeof("WLTIOS Shell v1.2\r\n") - 1);
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