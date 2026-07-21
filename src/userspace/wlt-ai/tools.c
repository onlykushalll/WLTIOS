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

/* Commands that the AI is never allowed to run.
 *
 * [ultrareview] This denylist is defense-in-depth, not a real security
 * boundary — it is trivially bypassed (whitespace variance like
 * "rm  -rf  /" with an extra space skips every substring match below;
 * command substitution can reconstruct any blocked string at runtime;
 * anything simply not on this list — arbitrary file exfiltration,
 * reading key material, curl|sh, etc. — passes through untouched).
 * A denylist over popen()/`/bin/sh -c` cannot be made safe by adding
 * more entries; the fix is architectural (route through wlt-permissions
 * like every other privileged action in this project, or drop shell
 * semantics the way wlt_tool_change_setting now does via execv). This
 * list is only here to stop the most catastrophic one-shot mistakes
 * until that fix lands — do not treat it as sufficient on its own. */
static const char *wlt_command_denylist[] = {
    "rm -rf /",
    "rm -fr /",
    "mkfs",
    "dd if=",
    "dd of=/dev/",
    "shutdown",
    "reboot",
    "poweroff",
    "halt",
    "init 0",
    "init 6",
    ":(){:|:&};:",
    "chmod 777 /",
    "chmod -r 777",
    "killall -9",
    "kill -9 1",
    "> /dev/sd",
    "> /dev/nvme",
    "of=/dev/sd",
    "of=/dev/nvme",
    "curl ",
    "wget ",
    "/etc/shadow",
    "/etc/passwd",
    "id_rsa",
    "id_ed25519",
    ".ssh/",
    "mcpilot",
    NULL
};

/* Normalize runs of whitespace to single spaces before denylist
 * matching, so "rm  -rf  /" (extra spaces) still matches "rm -rf /".
 * Closes the simplest bypass — does NOT make the denylist a real
 * security boundary, see the comment above. */
static void
wlt_normalize_whitespace(char *s)
{
    char *dst = s;
    bool in_space = false;
    for (const char *src = s; *src; src++) {
        if (*src == ' ' || *src == '\t') {
            if (!in_space) { *dst++ = ' '; in_space = true; }
        } else {
            *dst++ = *src; in_space = false;
        }
    }
    *dst = '\0';
}

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
    wlt_normalize_whitespace(lower); /* [ultrareview] see denylist comment */

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

/* Execute control_path with a single argv[1]=arg, no shell involved.
 * Replaces popen("%s %s", control_path, setting), which passed LLM-
 * controlled `setting` text straight to /bin/sh -c with no escaping —
 * e.g. setting="volume; rm -rf /home" would run a second, attacker/
 * model-chosen command. execv never invokes a shell — no metacharacter
 * to inject: the setting text is delivered as one argv entry, verbatim,
 * however it's spelled. [ultrareview C2/C3] */
static void
wlt_exec_argv_capture(const char *path, const char *arg,
                      wlt_ai_tool_result_t *result)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result->success = false;
        snprintf(result->output, sizeof(result->output),
                 "[pipe failed: %s]", strerror(errno));
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        result->success = false;
        snprintf(result->output, sizeof(result->output),
                 "[fork failed: %s]", strerror(errno));
        return;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        char *argv[3];
        argv[0] = (char *)path;
        argv[1] = (char *)arg;
        argv[2] = NULL;
        execv(path, argv);
        _exit(127);
    }

    close(pipefd[1]);
    size_t offset = 0;
    char buf[512];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        size_t len = (size_t)n;
        if (offset + len >= sizeof(result->output) - 1) {
            len = sizeof(result->output) - 1 - offset;
            if (len == 0) break;
        }
        memcpy(result->output + offset, buf, len);
        offset += len;
    }
    close(pipefd[0]);
    result->output[offset] = '\0';

    int status;
    waitpid(pid, &status, 0);
    result->success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);

    if (offset == 0) {
        snprintf(result->output, sizeof(result->output),
                 "[Command completed (exit %d)]",
                 WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
}

wlt_ai_tool_result_t
wlt_tool_change_setting(const char *setting)
{
    wlt_ai_tool_result_t result;
    const char *control_path;

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

    if (access(control_path, X_OK) != 0) {
        result.success = false;
        snprintf(result.output, sizeof(result.output),
                 "[Setting control not available: %s (%s)]",
                 setting, strerror(errno));
        return result;
    }

    wlt_exec_argv_capture(control_path, setting, &result);

    if (result.success) {
        /* Prepend status in a separate buffer — no in-place memmove
         * whose length math relied on an unenforced size relationship
         * between WLT_AI_MAX_TOOL_ARGS and WLT_AI_MAX_TOOL_RESULT. */
        char combined[WLT_AI_MAX_TOOL_RESULT];
        int n = snprintf(combined, sizeof(combined),
                         "[Setting changed: %s]\n%s", setting, result.output);
        if (n < 0) n = 0;
        if ((size_t)n >= sizeof(combined)) n = sizeof(combined) - 1;
        memcpy(result.output, combined, (size_t)n);
        result.output[n] = '\0';
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
