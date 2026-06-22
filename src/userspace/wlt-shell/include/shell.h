/**
 * @file shell.h
 * @brief Public API for WLT-Shell (desktop shell)
 * @module wlt-shell
 * @purpose Taskbar, app launcher, notifications, system tray
 * @dependencies: compositor, init
 * @tested_by: tests/userspace/shell_test.c
 * @spec: docs/specs/wlt-shell.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#ifndef WLT_SHELL_H
#define WLT_SHELL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_SHELL_OK = 0,
    WLT_SHELL_ERR_INVALID_ARG = -1,
    WLT_SHELL_ERR_IO = -2,
    WLT_SHELL_ERR_NOMEM = -3,
    WLT_SHELL_ERR_NOT_FOUND = -4,
    WLT_SHELL_ERR_PERMISSION = -5,
    WLT_SHELL_ERR_COMPOSITOR = -6,
} wlt_shell_error_t;

#define WLT_SHELL_VERSION "1.0.0"
#define WLT_SHELL_MAX_APPS 64
#define WLT_SHELL_MAX_NAME 128
#define WLT_SHELL_MAX_PATH 256
#define WLT_SHELL_MAX_NOTIFICATIONS 5
#define WLT_SHELL_NOTIFICATION_TIMEOUT_MS 5000

typedef enum {
    WLT_SHELL_TRAY_TOR = 0,
    WLT_SHELL_TRAY_NETWORK = 1,
    WLT_SHELL_TRAY_BATTERY = 2,
    WLT_SHELL_TRAY_CLOCK = 3,
    WLT_SHELL_TRAY_COUNT
} wlt_shell_tray_id_t;

typedef struct {
    char app_name[WLT_SHELL_MAX_NAME];
    char title[WLT_SHELL_MAX_NAME];
    char message[512];
    uint64_t timestamp;
    bool dismissed;
} wlt_shell_notification_t;

typedef struct {
    char name[WLT_SHELL_MAX_NAME];
    char exec_path[WLT_SHELL_MAX_PATH];
    char icon_path[WLT_SHELL_MAX_PATH];
    bool is_running;
    uint32_t pid;
} wlt_shell_app_t;

typedef struct {
    wlt_shell_app_t apps[WLT_SHELL_MAX_APPS];
    size_t app_count;
    wlt_shell_notification_t notifications[WLT_SHELL_MAX_NOTIFICATIONS];
    size_t notification_count;
    char tray_status[WLT_SHELL_TRAY_COUNT][64];
    bool launcher_visible;
    char launcher_query[WLT_SHELL_MAX_NAME];
    bool show_hidden;
} wlt_shell_state_t;

wlt_shell_error_t wlt_shell_init(wlt_shell_state_t *state);
wlt_shell_error_t wlt_shell_run(wlt_shell_state_t *state);
void wlt_shell_shutdown(wlt_shell_state_t *state);

wlt_shell_error_t wlt_shell_launcher_toggle(wlt_shell_state_t *state);
wlt_shell_error_t wlt_shell_launcher_search(wlt_shell_state_t *state, const char *query);
wlt_shell_error_t wlt_shell_launcher_launch(wlt_shell_state_t *state, const char *app_name);

wlt_shell_error_t wlt_shell_notification_show(wlt_shell_state_t *state,
                                               const char *app, const char *title,
                                               const char *message);
wlt_shell_error_t wlt_shell_notification_dismiss(wlt_shell_state_t *state, size_t index);

wlt_shell_error_t wlt_shell_tray_update(wlt_shell_state_t *state,
                                         wlt_shell_tray_id_t tray,
                                         const char *status);
wlt_shell_error_t wlt_shell_tray_get(const wlt_shell_state_t *state,
                                      wlt_shell_tray_id_t tray,
                                      char *status, size_t max_len);

wlt_shell_error_t wlt_shell_taskbar_update_apps(wlt_shell_state_t *state);
wlt_shell_error_t wlt_shell_taskbar_switch_app(wlt_shell_state_t *state, const char *app_name);

const char *wlt_shell_error_str(wlt_shell_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_SHELL_H */
