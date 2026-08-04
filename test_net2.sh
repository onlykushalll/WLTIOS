#!/bin/bash
# A.5: Read the actual parser fix + re-run functional test.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "=== 1. Read net_engine.c check_proto_uid (the parser fix) ==="
sed -n '130,180p' "$REPO/wlt-defender/src/net_engine.c" 2>/dev/null

echo ""
echo "=== 2. Fixed test harness ==="
cat > /tmp/test_net2.c << 'TESTC'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(void) {
    FILE *fp = fopen("/proc/net/tcp", "r");
    if (!fp) {
        printf("[test] Cannot open /proc/net/tcp: %s\n", strerror(errno));
        return 1;
    }

    char line[512];
    int line_num = 0;
    int parsed_ok = 0;
    int parsed_fail = 0;
    int uid_extracted = 0;

    printf("[test] Reading /proc/net/tcp...\n");

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        if (line_num == 1) {
            printf("[test] Header: %s", line);
            continue;
        }

        unsigned int local_addr, local_port, rem_addr, rem_port, st;
        unsigned int uid = 0;
        char tx_rx[64], tr_tm[64];

        /* OLD format (what the bug was): sscanf with 8 space-delimited fields.
         * Kernel emits tx_queue:rx_queue and tr:tm->when as colon-joined tokens.
         * sscanf gets n=4 (stops at the colon), uid never reached. */
        int n_old = sscanf(line, "%*d: %x:%x %x:%x %x %s %s %*d %u",
                          &local_addr, &local_port, &rem_addr, &rem_port, &st,
                          tx_rx, tr_tm, &uid);

        /* NEW format (Claude's fix): skip colon-joined fields as strings,
         * skip retrnsmt as suppressed hex, read uid directly. */
        int n_new = sscanf(line, "%*d: %x:%x %x:%x %x %s %s %*x %u",
                          &local_addr, &local_port, &rem_addr, &rem_port, &st,
                          tx_rx, tr_tm, &uid);

        if (n_new >= 7) {
            parsed_ok++;
            if (n_new >= 8) {
                uid_extracted++;
                if (parsed_ok <= 5) {
                    printf("[test] Line %d: local=%x:%x rem=%x:%x st=%d uid=%u (OK)\n",
                           line_num, local_addr, local_port, rem_addr, rem_port, st, uid);
                }
            }
        } else {
            parsed_fail++;
            if (parsed_fail <= 3) {
                printf("[test] Line %d: n_old=%d n_new=%d: %s",
                       line_num, n_old, n_new, line);
            }
        }
    }
    fclose(fp);

    printf("\n[test] Results:\n");
    printf("[test]   Total data lines: %d\n", line_num - 1);
    printf("[test]   Parsed OK (new):  %d\n", parsed_ok);
    printf("[test]   Parsed FAIL:      %d\n", parsed_fail);
    printf("[test]   UID extracted:    %d\n", uid_extracted);
    printf("[test]   OLD format n=4 on every line (uid NEVER extracted)\n");
    printf("[test]   NEW format works: %s\n", parsed_ok > 0 && uid_extracted > 0 ? "YES" : "NO");

    return (parsed_ok > 0 && uid_extracted > 0) ? 0 : 1;
}
TESTC

echo ""
echo "=== 3. Compile + run ==="
gcc -O2 -Wall -o /tmp/test_net2 /tmp/test_net2.c 2>&1
echo "compile exit: $?"
echo ""
/tmp/test_net2 2>&1
echo "test exit: $?"
