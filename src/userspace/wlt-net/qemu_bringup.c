/**
 * @file qemu_bringup.c
 * @brief QEMU network bringup — raw ioctl() implementation (no external ip/iptables)
 * @module wlt-net
 * @purpose Bring eth0 up with hardcoded QEMU user-mode networking defaults
 *          (10.0.2.15/24, gw 10.0.2.2). Used in the minimal initramfs where
 *          iproute2/iptables/dhclient are not available.
 * @dependencies: net.h
 * @spec: docs/specs/wlt-net.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 *
 * This is the QEMU/Stage-2 bringup path. The production wlt-net uses
 * ethernet.c (which calls external `ip`/`iptables`). This file is the
 * self-contained fallback for the minimal initramfs.
 *
 * QEMU user-mode networking defaults:
 *   - Guest IP:   10.0.2.15
 *   - Gateway:    10.0.2.2
 *   - DNS:        10.0.2.3
 *   - DHCP range: 10.0.2.15 to 10.0.2.31
 *
 * We hardcode 10.0.2.15/24 + gw 10.0.2.2 — no DHCP exchange needed.
 */

#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/route.h>

/* QEMU user-mode networking defaults */
#define QEMU_GUEST_IP   "10.0.2.15"
#define QEMU_NETMASK    "255.255.255.0"
#define QEMU_GATEWAY    "10.0.2.2"
#define QEMU_DNS        "10.0.2.3"

/* Try eth0 first, fall back to ens3 (systemd naming) */
static const char *const IFACE_CANDIDATES[] = { "eth0", "ens3", "enp0s3", NULL };

/* ─── Helpers ────────────────────────────────────────── */

/**
 * Check if a network interface exists in /sys/class/net/
 */
static bool
iface_exists(const char *ifname)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s", ifname);
    return access(path, F_OK) == 0;
}

/**
 * Find the first available interface from IFACE_CANDIDATES,
 * or fall back to the first non-lo interface in /sys/class/net/.
 * Returns a pointer that may point to static storage or strdup'd memory.
 * Caller must free if the returned pointer is not in IFACE_CANDIDATES.
 */
static char *
find_interface(void)
{
    /* Try the known candidates first */
    for (int i = 0; IFACE_CANDIDATES[i] != NULL; i++)
    {
        if (iface_exists(IFACE_CANDIDATES[i]))
            return strdup(IFACE_CANDIDATES[i]);
    }

    /* Fall back: scan /sys/class/net/ for the first non-lo interface */
    DIR *d = opendir("/sys/class/net");
    if (d == NULL)
        return NULL;

    struct dirent *entry;
    char *found = NULL;
    while ((entry = readdir(d)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (strcmp(entry->d_name, "lo") == 0)
            continue;
        found = strdup(entry->d_name);
        break;
    }
    closedir(d);
    return found;
}

/* ─── Interface bringup via ioctl() ──────────────────── */

/**
 * Set the IPv4 address on an interface.
 * Returns 0 on success, -errno on failure.
 */
static int
iface_set_ip(int sock, const char *ifname, const char *ip)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &addr->sin_addr) != 1)
        return -EINVAL;

    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0)
        return -errno;
    return 0;
}

/**
 * Set the netmask on an interface.
 */
static int
iface_set_netmask(int sock, const char *ifname, const char *mask)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, mask, &addr->sin_addr) != 1)
        return -EINVAL;

    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0)
        return -errno;
    return 0;
}

/**
 * Bring interface UP | RUNNING.
 */
static int
iface_set_up(int sock, const char *ifname)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    /* First read current flags */
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
        return -errno;

    /* Set UP and RUNNING */
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0)
        return -errno;
    return 0;
}

/**
 * Set the default route via gateway.
 */
static int
set_default_route(int sock, const char *gateway_ip, const char *ifname)
{
    struct rtentry route;
    memset(&route, 0, sizeof(route));

    /* Gateway */
    struct sockaddr_in *gw = (struct sockaddr_in *)&route.rt_gateway;
    gw->sin_family = AF_INET;
    if (inet_pton(AF_INET, gateway_ip, &gw->sin_addr) != 1)
        return -EINVAL;

    /* Destination: 0.0.0.0 (default route) */
    struct sockaddr_in *dst = (struct sockaddr_in *)&route.rt_dst;
    dst->sin_family = AF_INET;
    dst->sin_addr.s_addr = INADDR_ANY;

    /* Genmask: 0.0.0.0 (default) */
    struct sockaddr_in *mask = (struct sockaddr_in *)&route.rt_genmask;
    mask->sin_family = AF_INET;
    mask->sin_addr.s_addr = INADDR_ANY;

    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_dev = (char *)ifname;  /* Linux extension: bind route to device */

    if (ioctl(sock, SIOCADDRT, &route) < 0)
    {
        /* EEXIST means the route already exists — not an error */
        if (errno == EEXIST)
            return 0;
        return -errno;
    }
    return 0;
}

/* ─── Public API ─────────────────────────────────────── */

/**
 * Bring up the QEMU network interface with hardcoded defaults.
 *
 * This is the entry point called from the wlt-net service main in
 * build-initramfs.sh. It:
 *   1. Finds the interface (eth0 / ens3 / first non-lo)
 *   2. Sets IP 10.0.2.15, netmask 255.255.255.0
 *   3. Brings it UP | RUNNING
 *   4. Adds default route via 10.0.2.2
 *   5. Writes /etc/resolv.conf with DNS 10.0.2.3
 *
 * Prints progress to stdout (visible in QEMU serial output).
 *
 * Returns 0 on success, non-zero on failure.
 */
int
wlt_net_qemu_bringup(void)
{
    write(1, "[wlt-net] Bringing up QEMU network...\r\n", 39);

    /* Step 0: bring up loopback FIRST (Tor needs 127.0.0.1 to bind SOCKS) */
    {
        int lo_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (lo_sock >= 0)
        {
            /* Set 127.0.0.1/8 on lo */
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
            struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
            addr->sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr);
            ioctl(lo_sock, SIOCSIFADDR, &ifr);

            /* Set netmask 255.0.0.0 */
            struct sockaddr_in *mask = (struct sockaddr_in *)&ifr.ifr_netmask;
            mask->sin_family = AF_INET;
            inet_pton(AF_INET, "255.0.0.0", &mask->sin_addr);
            ioctl(lo_sock, SIOCSIFNETMASK, &ifr);

            /* Bring UP | RUNNING */
            if (ioctl(lo_sock, SIOCGIFFLAGS, &ifr) == 0)
            {
                ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
                ioctl(lo_sock, SIOCSIFFLAGS, &ifr);
            }
            close(lo_sock);
            write(1, "[wlt-net] Loopback (lo) UP at 127.0.0.1\r\n", 41);
        }
    }

    /* Step 1: find the interface */
    char *iface = find_interface();
    if (iface == NULL)
    {
        write(1, "[wlt-net] ERROR: no network interface found\r\n", 44);
        return 1;
    }

    {
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-net] Using interface: %s\r\n", iface);
        write(1, msg, n > 0 ? n : 0);
    }

    /* Step 2: open the ioctl socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        write(1, "[wlt-net] ERROR: cannot create socket\r\n", 38);
        free(iface);
        return 2;
    }

    /* Step 3: set IP address */
    int rc = iface_set_ip(sock, iface, QEMU_GUEST_IP);
    if (rc != 0)
    {
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-net] ERROR: SIOCSIFADDR %s: %s\r\n",
                         QEMU_GUEST_IP, strerror(-rc));
        write(1, msg, n > 0 ? n : 0);
        close(sock);
        free(iface);
        return 3;
    }
    write(1, "[wlt-net] IP 10.0.2.15 set\r\n", 28);

    /* Step 4: set netmask */
    rc = iface_set_netmask(sock, iface, QEMU_NETMASK);
    if (rc != 0)
    {
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-net] WARN: SIOCSIFNETMASK: %s\r\n",
                         strerror(-rc));
        write(1, msg, n > 0 ? n : 0);
        /* non-fatal — interface may still work */
    }

    /* Step 5: bring interface up */
    rc = iface_set_up(sock, iface);
    if (rc != 0)
    {
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-net] ERROR: SIOCSIFFLAGS: %s\r\n",
                         strerror(-rc));
        write(1, msg, n > 0 ? n : 0);
        close(sock);
        free(iface);
        return 4;
    }
    write(1, "[wlt-net] Interface UP\r\n", 24);

    /* Step 6: set default route */
    rc = set_default_route(sock, QEMU_GATEWAY, iface);
    if (rc != 0)
    {
        char msg[128];
        int n = snprintf(msg, sizeof(msg),
                         "[wlt-net] WARN: default route via %s: %s\r\n",
                         QEMU_GATEWAY, strerror(-rc));
        write(1, msg, n > 0 ? n : 0);
        /* non-fatal — network may still work for local subnet */
    }
    else
    {
        write(1, "[wlt-net] Default route via 10.0.2.2\r\n", 37);
    }

    /* Step 7: write /etc/resolv.conf for DNS */
    FILE *f = fopen("/etc/resolv.conf", "w");
    if (f != NULL)
    {
        fprintf(f, "nameserver %s\n", QEMU_DNS);
        fclose(f);
        write(1, "[wlt-net] DNS 10.0.2.3 configured\r\n", 35);
    }

    close(sock);
    free(iface);

    write(1, "[wlt-net] Network UP (10.0.2.15/24 gw 10.0.2.2)\r\n", 49);
    return 0;
}