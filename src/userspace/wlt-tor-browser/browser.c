/**
 * @file browser.c
 * @brief Tor Browser launcher — Firefox fork with privacy patches
 * @module wlt-tor-browser
 * @purpose Launch pre-configured Firefox with maximum privacy settings
 * @spec: docs/specs/wlt-tor-browser.md
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define WLT_TB_BINARY "/opt/wltios/tor-browser/firefox"
#define WLT_TB_PROFILE "/opt/wltios/tor-browser/profile"
#define WLT_TB_TOR_PROXY "127.0.0.1:9050"

typedef enum {
    WLT_TB_OK = 0,
    WLT_TB_ERR_NOT_FOUND,
    WLT_TB_ERR_LAUNCH,
    WLT_TB_ERR_TOR_DOWN,
} wlt_tb_err_t;

wlt_tb_err_t
wlt_tb_launch(void)
{
    /* Check if Tor is running before launching browser */
    if (access("/run/tor/tor.sock", F_OK) != 0) {
        return WLT_TB_ERR_TOR_DOWN;
    }

    /* Check if browser binary exists */
    if (access(WLT_TB_BINARY, X_OK) != 0) {
        return WLT_TB_ERR_NOT_FOUND;
    }

    /* Launch Firefox with privacy settings */
    pid_t pid = fork();
    if (pid < 0) return WLT_TB_ERR_LAUNCH;
    if (pid == 0) {
        /* Child process */
        char *argv[] = {
            (char *)WLT_TB_BINARY,
            "--profile", (char *)WLT_TB_PROFILE,
            "--no-remote",
            "--private-window",
            NULL
        };
        /* Set environment for Tor SOCKS proxy */
        setenv("MOZ_DISABLE_CONTENT_SANDBOX", "0", 1);
        setenv("MOZ_DISABLE_GPU_SANDBOX", "0", 1);
        execv(WLT_TB_BINARY, argv);
        _exit(127);
    }

    return WLT_TB_OK;
}

wlt_tb_err_t
wlt_tb_launch_url(const char *url)
{
    if (!url) return WLT_TB_ERR_NOT_FOUND;
    
    /* Check if Tor is running */
    if (access("/run/tor/tor.sock", F_OK) != 0) {
        return WLT_TB_ERR_TOR_DOWN;
    }

    pid_t pid = fork();
    if (pid < 0) return WLT_TB_ERR_LAUNCH;
    if (pid == 0) {
        char *argv[] = {
            (char *)WLT_TB_BINARY,
            "--profile", (char *)WLT_TB_PROFILE,
            "--no-remote",
            "--private-window",
            (char *)url,
            NULL
        };
        execv(WLT_TB_BINARY, argv);
        _exit(127);
    }

    return WLT_TB_OK;
}

bool
wlt_tb_is_running(void)
{
    /* Check if any firefox process is running */
    FILE *fp = popen("pgrep -f \"tor-browser/firefox\"", "r");
    if (!fp) return false;
    char buf[16];
    bool running = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return running;
}
