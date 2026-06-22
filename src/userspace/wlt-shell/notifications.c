/**
 * @file notifications.c
 * @brief Notification queue + display
 * @module wlt-shell/notifications
 * @purpose Show and manage desktop notifications
 * @dependencies: shell.h
 * @spec: docs/specs/wlt-shell.md
 */

#include "shell.h"
#include <string.h>
#include <time.h>

wlt_shell_error_t
wlt_shell_notification_show(wlt_shell_state_t *state,
                             const char *app, const char *title,
                             const char *message)
{
    if (state == NULL || app == NULL || title == NULL || message == NULL)
        return WLT_SHELL_ERR_INVALID_ARG;

    /* If queue is full, remove oldest */
    if (state->notification_count >= WLT_SHELL_MAX_NOTIFICATIONS)
    {
        /* Shift all notifications left */
        for (size_t i = 1; i < WLT_SHELL_MAX_NOTIFICATIONS; i++)
        {
            state->notifications[i - 1] = state->notifications[i];
        }
        state->notification_count = WLT_SHELL_MAX_NOTIFICATIONS - 1;
    }

    /* Add new notification */
    wlt_shell_notification_t *n = &state->notifications[state->notification_count];
    memset(n, 0, sizeof(*n));
    strncpy(n->app_name, app, WLT_SHELL_MAX_NAME - 1);
    strncpy(n->title, title, WLT_SHELL_MAX_NAME - 1);
    strncpy(n->message, message, sizeof(n->message) - 1);
    n->timestamp = (uint64_t)time(NULL);
    n->dismissed = false;

    state->notification_count++;

    return WLT_SHELL_OK;
}

wlt_shell_error_t
wlt_shell_notification_dismiss(wlt_shell_state_t *state, size_t index)
{
    if (state == NULL || index >= state->notification_count)
        return WLT_SHELL_ERR_INVALID_ARG;

    /* Mark as dismissed */
    state->notifications[index].dismissed = true;

    /* Shift remaining notifications left */
    for (size_t i = index + 1; i < state->notification_count; i++)
    {
        state->notifications[i - 1] = state->notifications[i];
    }

    state->notification_count--;

    return WLT_SHELL_OK;
}
