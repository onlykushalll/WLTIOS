/**
 * @file service.c
 * @brief Service loading and management implementation
 * @module wlt-init/service
 * @purpose Load, start, stop, restart services defined in /etc/wlt/services/
 * @dependencies: service.h, log.h, init.h
 * @tested_by: tests/userspace/service_test.c
 * @spec: docs/specs/wlt-init.md
 */

#include "service.h"
#include "dependency.h"
#include "tor.h"
#include "log.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>

/* ─── INI Parser Helpers ─────────────────────────────── */

static char *
trim(char *str)
{
    /* Find first non-space */
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    /* Shift string to remove leading spaces */
    if (start != str)
        memmove(str, start, strlen(start) + 1);

    /* Trim trailing spaces */
    if (*str)
    {
        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
            *end-- = '\0';
    }
    return str;
}

static int
parse_kv(const char *line, char *key, int key_max, char *val, int val_max)
{
    const char *eq = strchr(line, '=');
    if (eq == NULL)
        return -1;

    int klen = eq - line;
    if (klen >= key_max)
        klen = key_max - 1;
    strncpy(key, line, klen);
    key[klen] = '\0';
    trim(key);

    const char *vstart = eq + 1;
    int vlen = strlen(vstart);
    if (vlen >= val_max)
        vlen = val_max - 1;
    strncpy(val, vstart, vlen);
    val[vlen] = '\0';
    trim(val);

    return 0;
}

/* Parse comma-separated dependency list */
static void
parse_dependencies(wlt_service_t *svc, const char *val)
{
    char buf[WLT_INIT_MAX_ARGS];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",");
    while (token != NULL && svc->depend_count < 8)
    {
        char *dep = trim(token);
        if (*dep != '\0')
        {
            strncpy(svc->depends[svc->depend_count], dep,
                    WLT_INIT_MAX_NAME - 1);
            svc->depends[svc->depend_count][WLT_INIT_MAX_NAME - 1] = '\0';
            svc->depend_count++;
        }
        token = strtok(NULL, ",");
    }
}

/* ─── Service File Parsing ───────────────────────────── */

static wlt_init_error_t
parse_service_file(const char *content, wlt_service_t *svc)
{
    char section[64] = "";
    char line[512];
    const char *ptr = content;

    /* Initialize service with defaults */
    svc->tor_required = false;
    svc->restart_on_crash = true;
    svc->restart_backoff_ms = WLT_INIT_RESTART_BACKOFF_MS;
    svc->priority = WLT_PRIO_NORMAL;
    svc->depend_count = 0;
    strcpy(svc->user, "wlt");

    while (*ptr)
    {
        /* Get one line */
        int i = 0;
        while (*ptr && *ptr != '\n' && i < (int)sizeof(line) - 1)
        {
            line[i++] = *ptr++;
        }
        line[i] = '\0';
        if (*ptr == '\n')
            ptr++;

        char *trimmed = trim(line);

        /* Skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        /* Section header */
        if (*trimmed == '[')
        {
            char *end = strchr(trimmed, ']');
            if (end)
            {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }

        /* Key = value */
        char key[128], val[300];
        if (parse_kv(trimmed, key, sizeof(key), val, sizeof(val)) == 0)
        {
            if (strcmp(section, "wlt-service") == 0 ||
                strcmp(section, "app") == 0)
            {
                if (strcmp(key, "name") == 0)
                {
                    strncpy(svc->name, val, WLT_INIT_MAX_NAME - 1);
                    svc->name[WLT_INIT_MAX_NAME - 1] = '\0';
                }
                else if (strcmp(key, "exec") == 0)
                {
                    strncpy(svc->exec, val, WLT_INIT_MAX_PATH - 1);
                    svc->exec[WLT_INIT_MAX_PATH - 1] = '\0';
                }
                else if (strcmp(key, "args") == 0)
                {
                    strncpy(svc->args, val, WLT_INIT_MAX_ARGS - 1);
                    svc->args[WLT_INIT_MAX_ARGS - 1] = '\0';
                }
                else if (strcmp(key, "user") == 0)
                {
                    strncpy(svc->user, val, sizeof(svc->user) - 1);
                    svc->user[sizeof(svc->user) - 1] = '\0';
                }
            }
            else if (strcmp(section, "permissions") == 0 ||
                     strcmp(section, "config") == 0)
            {
                if (strcmp(key, "depends") == 0)
                {
                    parse_dependencies(svc, val);
                }
                else if (strcmp(key, "tor_required") == 0)
                {
                    svc->tor_required = (strcmp(val, "yes") == 0 ||
                                          strcmp(val, "true") == 0);
                }
                else if (strcmp(key, "restart_on_crash") == 0)
                {
                    svc->restart_on_crash = (strcmp(val, "yes") == 0 ||
                                              strcmp(val, "true") == 0);
                }
                else if (strcmp(key, "restart_backoff_ms") == 0)
                {
                    svc->restart_backoff_ms = (uint32_t)atoi(val);
                }
                else if (strcmp(key, "priority") == 0)
                {
                    if (strcmp(val, "critical") == 0)
                        svc->priority = WLT_PRIO_CRITICAL;
                    else if (strcmp(val, "low") == 0)
                        svc->priority = WLT_PRIO_LOW;
                    else
                        svc->priority = WLT_PRIO_NORMAL;
                }
            }
        }
    }

    return WLT_INIT_OK;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_init_error_t
wlt_service_load_all(wlt_init_t *init)
{
    DIR *dir;
    struct dirent *entry;
    char path[WLT_INIT_MAX_PATH];

    if (init == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    init->service_count = 0;

    dir = opendir(WLT_INIT_SERVICE_DIR);
    if (dir == NULL)
    {
        WLT_LOG_WRN("Service directory not found: %s", WLT_INIT_SERVICE_DIR);
        return WLT_INIT_ERR_NOT_FOUND;
    }

    while ((entry = readdir(dir)) != NULL &&
           init->service_count < WLT_INIT_MAX_SERVICES)
    {
        /* Skip files that don't end in .service */
        const char *ext = strrchr(entry->d_name, '.');
        if (ext == NULL || strcmp(ext, ".service") != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s",
                 WLT_INIT_SERVICE_DIR, entry->d_name);

        wlt_init_error_t err = wlt_service_load(path,
            &init->services[init->service_count]);
        if (err == WLT_INIT_OK)
        {
            init->services[init->service_count].state = WLT_SVC_STOPPED;
            init->services[init->service_count].pid = -1;
            init->services[init->service_count].restart_count = 0;
            init->service_count++;
            WLT_LOG_DBG("Loaded service: %s",
                         init->services[init->service_count - 1].name);
        }
        else
        {
            WLT_LOG_ERR("Failed to load %s: %s", path,
                         wlt_init_error_str(err));
        }
    }

    closedir(dir);
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_service_load(const char *path, wlt_service_t *service)
{
    if (path == NULL || service == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return WLT_INIT_ERR_IO;

    /* Read file content */
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    /* Parse */
    wlt_init_error_t err = parse_service_file(buf, service);
    if (err != WLT_INIT_OK)
        return err;

    /* Validate */
    if (service->name[0] == '\0')
    {
        WLT_LOG_ERR("Service has no name: %s", path);
        return WLT_INIT_ERR_PARSE;
    }
    if (service->exec[0] == '\0')
    {
        WLT_LOG_ERR("Service has no exec: %s", path);
        return WLT_INIT_ERR_PARSE;
    }

    return WLT_INIT_OK;
}

wlt_service_t *
wlt_service_find(wlt_init_t *init, const char *name)
{
    if (init == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < init->service_count; i++)
    {
        if (strcmp(init->services[i].name, name) == 0)
        {
            return &init->services[i];
        }
    }
    return NULL;
}

wlt_init_error_t
wlt_service_start(wlt_init_t *init, wlt_service_t *svc)
{
    if (init == NULL || svc == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    if (svc->state == WLT_SVC_RUNNING)
    {
        WLT_LOG_WRN("Service %s already running", svc->name);
        return WLT_INIT_OK;
    }

    /* Check dependencies */
    if (!wlt_dependency_deps_running(init, svc))
    {
        WLT_LOG_WRN("Cannot start %s — dependencies not ready", svc->name);
        return WLT_INIT_ERR_TOR_NOT_READY;
    }

    /* Check Tor if required */
    if (svc->tor_required && !wlt_tor_is_ready(init))
    {
        WLT_LOG_WRN("Cannot start %s — Tor not ready", svc->name);
        return WLT_INIT_ERR_TOR_NOT_READY;
    }

    WLT_LOG_INF("Starting %s: %s %s", svc->name, svc->exec, svc->args);

    svc->state = WLT_SVC_STARTING;

    pid_t pid = fork();
    if (pid < 0)
    {
        WLT_LOG_ERR("fork() failed for %s", svc->name);
        svc->state = WLT_SVC_FAILED;
        return WLT_INIT_ERR_IO;
    }

    if (pid == 0)
    {
        /* Child process */
        /* TODO(wlt-init): Set up cgroup, namespace, seccomp via wlt-sandbox */
        /* TODO(wlt-init): Drop privileges to svc->user */

        /* Parse args (simple space-split) */
        char *argv[32];
        int argc = 0;
        char args_copy[WLT_INIT_MAX_ARGS];
        strncpy(args_copy, svc->args, sizeof(args_copy) - 1);
        args_copy[sizeof(args_copy) - 1] = '\0';

        argv[argc++] = svc->exec;
        char *token = strtok(args_copy, " ");
        while (token != NULL && argc < 31)
        {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }
        argv[argc] = NULL;

        execv(svc->exec, argv);
        /* If execv returns, it failed */
        WLT_LOG_ERR("execv() failed for %s", svc->exec);
        _exit(127);
    }

    /* Parent process */
    svc->pid = pid;
    svc->state = WLT_SVC_RUNNING;
    svc->started_at = (uint64_t)time(NULL);
    WLT_LOG_INF("Started %s (PID %d)", svc->name, pid);

    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_service_stop(wlt_init_t *init, wlt_service_t *svc)
{
    if (init == NULL || svc == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    if (svc->state != WLT_SVC_RUNNING && svc->state != WLT_SVC_FROZEN)
    {
        WLT_LOG_WRN("Service %s not running", svc->name);
        return WLT_INIT_OK;
    }

    WLT_LOG_INF("Stopping %s (PID %d)", svc->name, svc->pid);

    svc->state = WLT_SVC_STOPPING;

    /* Send SIGTERM */
    kill(svc->pid, SIGTERM);

    /* Wait up to 5 seconds */
    int status;
    for (int i = 0; i < 50; i++)
    {
        pid_t ret = waitpid(svc->pid, &status, WNOHANG);
        if (ret == svc->pid)
            break;
        usleep(100000);  /* 100ms */
    }

    /* If still running, SIGKILL */
    if (waitpid(svc->pid, &status, WNOHANG) == 0)
    {
        WLT_LOG_WRN("Service %s didn't stop, sending SIGKILL", svc->name);
        kill(svc->pid, SIGKILL);
        waitpid(svc->pid, &status, 0);
    }

    svc->state = WLT_SVC_STOPPED;
    svc->pid = -1;
    WLT_LOG_INF("Stopped %s", svc->name);

    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_service_restart(wlt_init_t *init, wlt_service_t *svc)
{
    wlt_service_stop(init, svc);
    return wlt_service_start(init, svc);
}

const char *
wlt_service_state_str(wlt_service_state_t state)
{
    switch (state)
    {
        case WLT_SVC_STOPPED:  return "stopped";
        case WLT_SVC_STARTING: return "starting";
        case WLT_SVC_RUNNING:  return "running";
        case WLT_SVC_STOPPING: return "stopping";
        case WLT_SVC_FAILED:   return "failed";
        case WLT_SVC_FROZEN:   return "frozen";
        default:               return "unknown";
    }
}
