/**
 * wlt-notify.c — WLTIOS Notification Push Tool (FR-GUI-08)
 * @purpose Push a notification to the compositor's notification overlay.
 *          Writes to /run/wlt-notify which the compositor reads on each frame.
 * @usage: wlt-notify "Update available: v2.0.0"
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define NOTIFY_FILE "/run/wlt-notify"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: wlt-notify <message>\n");
        fprintf(stderr, "Pushes a notification to the compositor overlay.\n");
        return 1;
    }
    
    /* Ensure /run exists */
    mkdir("/run", 0755);
    
    FILE *f = fopen(NOTIFY_FILE, "w");
    if (!f) {
        perror("wlt-notify: cannot write to " NOTIFY_FILE);
        return 1;
    }
    
    /* Write the notification message */
    fprintf(f, "%s\n", argv[1]);
    fclose(f);
    
    fprintf(stderr, "[wlt-notify] Notification sent: %s\n", argv[1]);
    return 0;
}
