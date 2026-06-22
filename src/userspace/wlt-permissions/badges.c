/**
 * @file badges.c
 * @brief Privacy badge calculation for WLT-Permissions
 * @module wlt-permissions/badges
 * @purpose Derives a green/yellow/red privacy risk badge from an app's
 *          effective permission grants.
 * @dependencies: manifest.h, permissions.h
 * @tested_by: tests/userspace/permissions_badge_test.c
 * @spec: docs/specs/wlt-permissions.md
 *
 * Badge policy (spec Â§Privacy Badges, completed for the sensor gap the
 * spec leaves between green and red):
 *
 *   RED    full filesystem, OR camera+microphone together, OR
 *          background+autostart together (silent persistence).
 *   YELLOW direct network, OR home:rw, OR clipboard, OR any single
 *          sensitive sensor (camera, microphone, or location). The
 *          spec defines green as "no camera/mic/location", so a lone
 *          sensor cannot be green; red needs a pair, so it lands here.
 *   GREEN  tor-only or no network, read-only or no filesystem, and no
 *          sensitive sensors or clipboard.
 */

#include "permissions.h"

#include <string.h>

static wlt_perms_value_t
g(const wlt_PermsApp *app, wlt_perms_id_t id)
{
    return app->grant[id];
}

wlt_perms_badge_t
wlt_perms_badge_for_app(const wlt_PermsApp *app)
{
    if (app == NULL)
        return WLT_PERMS_BADGE_GREEN;

    bool cam_on = (g(app, WLT_PERM_CAMERA) == WLT_PERMS_ALLOW);
    bool mic_on = (g(app, WLT_PERM_MICROPHONE) == WLT_PERMS_ALLOW);
    bool loc_on = (g(app, WLT_PERM_LOCATION) == WLT_PERMS_ALLOW);

    /* RED â high risk. */
    if (g(app, WLT_PERM_FILESYSTEM) == WLT_PERMS_FS_FULL)
        return WLT_PERMS_BADGE_RED;
    if (cam_on && mic_on)
        return WLT_PERMS_BADGE_RED;
    if (g(app, WLT_PERM_BACKGROUND) == WLT_PERMS_ALLOW &&
        g(app, WLT_PERM_AUTOSTART) == WLT_PERMS_ALLOW)
        return WLT_PERMS_BADGE_RED;

    /* YELLOW â review recommended. */
    if (g(app, WLT_PERM_NETWORK) == WLT_PERMS_NET_DIRECT)
        return WLT_PERMS_BADGE_YELLOW;
    if (g(app, WLT_PERM_FILESYSTEM) == WLT_PERMS_FS_HOME_RW)
        return WLT_PERMS_BADGE_YELLOW;
    if (g(app, WLT_PERM_CLIPBOARD) == WLT_PERMS_ALLOW)
        return WLT_PERMS_BADGE_YELLOW;
    if (cam_on || mic_on || loc_on)
        return WLT_PERMS_BADGE_YELLOW;

    /* GREEN â tor-only or offline, no sensitive sensors or clipboard. */
    return WLT_PERMS_BADGE_GREEN;
}

wlt_perms_badge_t
wlt_perms_badge(const wlt_PermsManager *m, const char *app_id)
{
    if (m == NULL || app_id == NULL)
        return WLT_PERMS_BADGE_GREEN;

    const wlt_PermsApp *app = NULL;
    for (size_t i = 0; i < m->app_count; i++)
    {
        if (strcmp(m->apps[i].id, app_id) == 0)
        {
            app = &m->apps[i];
            break;
        }
    }
    if (app == NULL)
        return WLT_PERMS_BADGE_GREEN;  /* unknown app â no risk surface */
    return wlt_perms_badge_for_app(app);
}

const char *
wlt_perms_badge_str(wlt_perms_badge_t b)
{
    switch (b)
    {
        case WLT_PERMS_BADGE_GREEN:  return "green";
        case WLT_PERMS_BADGE_YELLOW: return "yellow";
        case WLT_PERMS_BADGE_RED:    return "red";
        default:                     return "?";
    }
}

const char *
wlt_perms_badge_emoji(wlt_perms_badge_t b)
{
    switch (b)
    {
        case WLT_PERMS_BADGE_GREEN:  return "\xF0\x9F\x9F\xA2";  /* ð¢ */
        case WLT_PERMS_BADGE_YELLOW: return "\xF0\x9F\x9F\xA1";  /* ð¡ */
        case WLT_PERMS_BADGE_RED:    return "\xF0\x9F\x94\xB4";  /* ð´ */
        default:                     return "?";
    }
}
