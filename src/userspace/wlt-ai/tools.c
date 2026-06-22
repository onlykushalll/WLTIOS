/*
 * tools.c - WLT-AI System Control Tools
 *
 * Implements the tool-use layer that lets the AI assistant control
 * the operating system. Each tool function receives a string argument
 * (parsed from the LLM output) and returns a structured result.
 *
 * Tools implemented:
 *   - wlt_tool_open_app       Launch an application
 *   - wlt_tool_change_setting Modify a system setting (volume, wifi, etc.)
 *   - wlt_tool_run_command    Execute a shell command
 *   - wlt_tool_file_search    Search the filesystem for matching files
 *
 * Security: Commands run in the AI's sandbox with no special privileges.
 * Shell commands are validated against a denylist before execution.
 *
 * Copyright (c) WLTIOS Project. All rights reserved.
 */

#include "ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/* Known application registry â maps friendly names to binaries */
typedef struct {
    const char *name;
    const char *binary;
} wlt_app_entry_t;

static const wlt_app_entry_t wlt_app_registry[] = {
    { "firefox",    "firefox" },
    { "browser",    "firefox" },
    { "chrome",     "chromium" },
    { "terminal",   "wlt-terminal" },
    { "files",      "wlt-filemanager" },
    { "file manager", "wlt-filemanager" },
    { "settings",   "wlt-settings" },
    { "editor",     "wlt-editor" },
    { "code",       "wlt-editor" },
    { "mail",       "thunderbird" },
    { "music",      "wlt-music" },
    { "calculator", "wlt-calc" },
    { "camera",     "wlt-camera" },
    { "notes",      "wlt-notes" },
    { "calendar",   "wlt-calendar" },
    { "vpn",        "wlt-vpn" },
    { "tor",        "wlt-tor" },
    { NULL, NULL }
};

/* Settings registry â maps setting keywords to control scripts */
typedef struct {
    const char *keyword;
    const char *control_path;
} wlt_setting_entry_t;

static const wlt_setting_entry_t wlt_setting_registry[] = {
    { "volume",     "/usr/lib/wltios/settings/volume" },
    { "brightness", "/usr/lib/wltios/settings/brightness" },
    { "wifi",       "/usr/lib/wltios/settings/wifi" },
    { "bluetooth",  "/usr/lib/wltios/settings/bluetooth" },
    { "vpn",        "/usr/lib/wltios/settings/vpn" },
    { "tor",        "/usr/lib/wltios/settings/tor" },
    { "killswitch", "/usr/lib/wltios/settings/killswitch" },
    { "airplane",   "/usr/lib/wltios/settings/airplane" },
    { "darkmode",   "/usr/lib/wltios/settings/darkmode" },
    { "nightmode",  "/usr/lib/wltios/settings/nightmode" },
    { "mac",        "/usr/lib/wltios/settings/macspoof" },
    { NULL, NULL }
};

/* Commands that the AI is never allowed to run */
static const char *wlt_command_denylist[] = {
    "rm -rf /",
    "mkfs",
    "dd if=",
    "shutdown",
    "reboot",
    "init 0",
    "init 6",
    ":(){:|:&};:",
    "chmod 777 /",
    "killall -9",
    "kill -9 1",
    NULL
};

static void
wlt_str_trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s))
        s++;
    if (*s == '\0') {
        return;
    }
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
}

static void
wlt_str_tolower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

static bool
wlt_command_is_safe(const char *cmd)
{
    size_t i;
    char lower[WLT_AI_CMD_LEN];

    strncpy(lower, cmd, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    wlt_str_tolower(lower);

    for (i = 0; wlt_command_denylist[i] != NULL; i++) {
        if (strstr(lower, wlt_command_denylist[i]) != NULL) {
            fprintf(stderr, "wlt-ai: blocked unsafe command: %s\n",
                    wlt_command_denylist[i]);
            return false;
        }
    }
    return true;
}

static const char *
wlt_lookup_app(const char *name)
{
    char lower[WLT_AI_APP_NAME_LEN];
    size_t i;

    strncpy(lower, name, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    wlt_str_tolower(lower);
    wlt_str_trim(lower);

    for (i = 0; wlt_app_registry[i].name != NULL; i++) {
        if (strcmp(lower, wlt_app_registry[i].name) == 0)
            return wlt_app_registry[i].binary;
    }
    return NULL;
}

static const char *
wlt_lookup_setting(const char *args)
{
    char lower[WLT_AI_SETTING_LEN];
    size_t i;

    strncpy(lower, args, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    wlt_str_tolower(lower);

    for (i = 0; wlt_setting_registry[i].keyword != NULL; i++) {
        if (strstr(lower, wlt_setting_registry[i].keyword) != NULL)
            return wlt_setting_registry[i].control_path;
    }
    return NULL;
}

/* Execute a command and capture stdout/stderr into result */
static void
wlt_exec_capture(const char *cmd, wlt_ai_tool_result_t *result)
{
    char buf[512];
    FILE *pipe;
    size_t offset = 0;

    pipe = popen(cmd, "r");
    if (pipe == NULL) {
        result->success = false;
        snprintf(result->output, sizeof(result->output),
                 "[Failed to execute: %s]", strerror(errno));
        return;
    }

    while (fgets(buf, sizeof(buf), pipe) != NULL) {
        size_t len = strlen(buf);
        if (offset + len >= sizeof(result->output) - 1)
            break;
        memcpy(result->output + offset, buf, len);
        offset += len;
    }
    result->output[offset] = '\0';

    int status = pclose(pipe);
    result->success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);

    if (offset == 0) {
        snprintf(result->output, sizeof(result->output),
                 "[Command completed (exit %d)]",
                 WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
}

/* ------------------------------------------------------------------ */
/* Tool: Open Application                                             */
/* ------------------------------------------------------------------ */

wlt_ai_tool_result_t
wlt_tool_open_app(const char *app_name)
{
    wlt_ai_tool_result_t result;
    const char *binary;
    char clean[WLT_AI_APP_NAME_LEN];
    pid_t pid;

    memset(&result, 0, sizeof(result));
    clean[0] = '\0';

    if (app_name == NULL || app_name[0] == '\0') {
        result.success = false;
        strncpy(result.output, "[No application specified]",
                sizeof(result.output) - 1);
        return result;
    }

    binary = wlt_lookup_app(app_name);
    if (binary == NULL) {
        /* Try using the name directly if it looks like a valid binary */
        strncpy(clean, app_name, sizeof(clean) - 1);
        clean[sizeof(clean) - 1] = '\0';
        wlt_str_trim(clean);

        /* Only allow alphanumeric + dash + underscore */
        bool valid = true;
        for (size_t i = 0; clean[i]; i++) {
            if (!isalnum((unsigned char)clean[i]) &&
                clean[i] != '-' && clean[i] != '_') {
                valid = false;
                break;
            }
        }

        if (!valid) {
            result.success = false;
            snprintf(result.output, sizeof(result.output),
                     "[Unknown application: %s]", app_name);
            return result;
        }
        binary = clean;
    }

    /* Fork and exec the application in the background */
    pid = fork();
    if (pid < 0) {
        result.success = false;
        snprintf(result.output, sizeof(result.output),
                 "[fork failed: %s]", strerror(errno));
        return result;
    }

    if (pid == 0) {
        /* Child process */
        execlp(binary, binary, (char *)NULL);
        /* If execlp returns, it failed */
        _exit(127);
    }

    /* Parent: don't wait â the app runs in the background */
    result.success = true;
    snprintf(result.output, sizeof(result.output),
             "[Launched: %s (pid %d)]", binary, (int)pid);
    return result;
}

/* ------------------------------------------------------------------ */
/* Tool: Change Setting                                               */
/* ------------------------------------------------------------------ */

wlt_ai_tool_result_t
wlt_tool_change_setting(const char *setting)
{
    wlt_ai_tool_result_t result;
    const char *control_path;
    char cmd[WLT_AI_CMD_LEN];

    memset(&result, 0, sizeof(result));

    if (setting == NULL || setting[0] == '\0') {
        result.success = false;
        strncpy(result.output, "[No setting specified]",
                sizeof(result.output) - 1);
        return result;
    }

    control_path = wlt_lookup_setting(setting);
    if (control_path == NULL) {
        result.success = false;
        snprintf(result.output, sizeof(result.output),
                 "[Unknown setting: %s]", setting);
        return result;
    }

    /* Check that the control script exists and is executable */
    if (access(control_path, X_OK) != 0) {
        result.success = false;
        snprintf(result.output, sizeof(result.output),
                 "[Setting control not available: %s (%s)]",
                 setting, strerror(errno));
        return result;
    }

    snprintf(cmd, sizeof(cmd), "%s %s", control_path, setting);
    wlt_exec_capture(cmd, &result);

    if (result.success) {
        char msg[WLT_AI_MAX_TOOL_RESULT];
        snprintf(msg, sizeof(msg), "[Setting changed: %s]", setting);
        /* Prepend status to output */
        memmove(result.output + strlen(msg) + 1, result.output,
                sizeof(result.output) - strlen(msg) - 1);
        memcpy(result.output, msg, strlen(msg));
        result.output[strlen(msg)] = '\n';
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* Tool: Run Command                                                  */
/* ------------------------------------------------------------------ */

wlt_ai_tool_result_t
wlt_tool_run_command(const char *command)
{
    wlt_ai_tool_result_t result;
    char clean[WLT_AI_CMD_LEN];

    memset(&result, 0, sizeof(result));

    if (command == NULL || command[0] == '\0') {
        result.success = false;
        strncpy(result.output, "[No command specified]",
                sizeof(result.output) - 1);
        return result;
    }

    strncpy(clean, command, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    wlt_str_trim(clean);

    /* Security: validate against denylist */
    if (!wlt_command_is_safe(clean)) {
        result.success = false;
        strncpy(result.output,
                "[Command blocked by safety policy]",
                sizeof(result.output) - 1);
        return result;
    }

    wlt_exec_capture(clean, &result);
    return result;
}

/* ------------------------------------------------------------------ */
/* Tool: File Search                                                  */
/* ------------------------------------------------------------------ */

/* Recursive directory walker â searches for files matching a pattern */
static void
wlt_search_dir(const char *dir, const char *pattern,
               wlt_ai_tool_result_t *result, size_t *offset, int depth)
{
    DIR *d;
    struct dirent *ent;
    char path[WLT_AI_FILE_PATH_LEN];
    struct stat st;

    if (depth > 5 || *offset >= sizeof(result->output) - 256)
        return;

    d = opendir(dir);
    if (d == NULL)
        return;

    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;

        /* Skip . and .. */
        if (name[0] == '.' && (name[1] == '\0' ||
            (name[1] == '.' && name[2] == '\0')))
            continue;

        /* Skip hidden files unless pattern starts with . */
        if (name[0] == '.' && pattern[0] != '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, name);

        /* Check if name matches pattern (case-insensitive substring) */
        {
            char lower_name[256];
            char lower_pat[256];
            size_t i;

            strncpy(lower_name, name, sizeof(lower_name) - 1);
            lower_name[sizeof(lower_name) - 1] = '\0';
            strncpy(lower_pat, pattern, sizeof(lower_pat) - 1);
            lower_pat[sizeof(lower_pat) - 1] = '\0';

            for (i = 0; lower_name[i]; i++)
                lower_name[i] = (char)tolower((unsigned char)lower_name[i]);
            for (i = 0; lower_pat[i]; i++)
                lower_pat[i] = (char)tolower((unsigned char)lower_pat[i]);

            if (strstr(lower_name, lower_pat) != NULL) {
                size_t len = strlen(path);
                if (*offset + len + 2 < sizeof(result->output)) {
                    memcpy(result->output + *offset, path, len);
                    *offset += len;
                    result->output[(*offset)++] = '\n';
                }
            }
        }

        /* Recurse into directories */
        if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Skip common large/irrelevant directories */
            if (strcmp(name, "node_modules") != 0 &&
                strcmp(name, ".git") != 0 &&
                strcmp(name, "proc") != 0 &&
                strcmp(name, "sys") != 0 &&
                strcmp(name, "dev") != 0) {
                wlt_search_dir(path, pattern, result, offset, depth + 1);
            }
        }
    }

    closedir(d);
}

wlt_ai_tool_result_t
wlt_tool_file_search(const char *query)
{
    wlt_ai_tool_result_t result;
    char pattern[WLT_AI_APP_NAME_LEN];
    size_t offset = 0;
    size_t result_count;

    memset(&result, 0, sizeof(result));

    if (query == NULL || query[0] == '\0') {
        result.success = false;
        strncpy(result.output, "[No search query specified]",
                sizeof(result.output) - 1);
        return result;
    }

    /* Extract the file pattern from the query.
     * The LLM passes a cleaned pattern, but we also handle raw NL. */
    strncpy(pattern, query, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = '\0';
    wlt_str_trim(pattern);

    /* Search common user directories */
    const char *search_dirs[] = {
        "/home",
        "/tmp",
        "/var/lib/wlt-ai",
        NULL
    };

    for (size_t i = 0; search_dirs[i] != NULL; i++) {
        wlt_search_dir(search_dirs[i], pattern, &result, &offset, 0);
    }

    /* Count results (lines) */
    result_count = 0;
    for (size_t i = 0; i < offset; i++) {
        if (result.output[i] == '\n')
            result_count++;
    }

    if (offset == 0) {
        result.success = true;
        snprintf(result.output, sizeof(result.output),
                 "[No files matching '%s' found]", pattern);
    } else {
        result.success = true;
        /* Truncate if too many results */
        if (result_count > 20) {
            /* Find the 20th newline and truncate */
            size_t count = 0;
            size_t cut = 0;
            for (size_t i = 0; i < offset; i++) {
                if (result.output[i] == '\n') {
                    count++;
                    if (count == 20) {
                        cut = i;
                        break;
                    }
                }
            }
            if (cut > 0) {
                result.output[cut] = '\0';
                offset = cut;
                snprintf(result.output + offset,
                         sizeof(result.output) - offset,
                         "... and %zu more results omitted\n",
                         result_count - 20);
            }
        }
    }

    return result;
}
