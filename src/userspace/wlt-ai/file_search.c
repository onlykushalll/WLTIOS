/**
 * @file file_search.c
 * @brief Natural language file search — "find my PDF from Monday"
 * @module wlt-ai/file_search
 * @purpose Parse natural language queries, search filesystem, return results
 */
struct wlt_ai_ctx; /* Forward declaration from ai.h */
#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define WLT_FS_MAX_RESULTS 50
#define WLT_FS_MAX_PATH    512
#define WLT_FS_MAX_QUERY   256

typedef struct {
    char path[WLT_FS_MAX_PATH];
    char name[256];
    time_t modified;
    size_t size;
} wlt_fs_result_t;

typedef struct {
    wlt_fs_result_t results[WLT_FS_MAX_RESULTS];
    size_t count;
} wlt_fs_search_ctx_t;

/* Parse natural language query into search parameters */
typedef struct {
    char extension[16];     /* "pdf", "doc", "" (any) */
    char name_contains[128]; /* keyword in filename */
    time_t after;           /* files modified after this time */
    time_t before;          /* files modified before this time */
    char search_dir[WLT_FS_MAX_PATH]; /* directory to search */
} wlt_fs_query_t;

static void
parse_query(const char *nl_query, wlt_fs_query_t *q)
{
    memset(q, 0, sizeof(*q));
    strncpy(q->search_dir, "/home", sizeof(q->search_dir) - 1);
    q->after = 0;
    q->before = 0;

    /* Check for file type keywords */
    if (strstr(nl_query, "pdf") || strstr(nl_query, "PDF"))
        strncpy(q->extension, "pdf", sizeof(q->extension) - 1);
    else if (strstr(nl_query, "doc") || strstr(nl_query, "document"))
        strncpy(q->extension, "doc", sizeof(q->extension) - 1);
    else if (strstr(nl_query, "image") || strstr(nl_query, "picture") || strstr(nl_query, "photo"))
        strncpy(q->extension, "png", sizeof(q->extension) - 1);
    else if (strstr(nl_query, "video"))
        strncpy(q->extension, "mp4", sizeof(q->extension) - 1);

    /* Check for time keywords */
    time_t now = time(NULL);
    if (strstr(nl_query, "today"))
        q->after = now - 86400;
    else if (strstr(nl_query, "yesterday")) {
        q->after = now - 172800;
        q->before = now - 86400;
    } else if (strstr(nl_query, "monday") || strstr(nl_query, "Monday"))
        q->after = now - 604800; /* 7 days ago */
    else if (strstr(nl_query, "week") || strstr(nl_query, "last week"))
        q->after = now - 604800;
    else if (strstr(nl_query, "month") || strstr(nl_query, "last month"))
        q->after = now - 2592000;

    /* Extract name keywords (simple: words between "called" and end) */
    const char *called = strstr(nl_query, "called ");
    if (called) {
        called += 7;
        const char *end = strchr(called, ' ');
        size_t len = end ? (size_t)(end - called) : strlen(called);
        if (len > 0 && len < sizeof(q->name_contains))
            strncpy(q->name_contains, called, len);
    }
}

static void
search_dir(const char *dir, const wlt_fs_query_t *q, wlt_fs_search_ctx_t *ctx)
{
    if (ctx->count >= WLT_FS_MAX_RESULTS) return;

    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && ctx->count < WLT_FS_MAX_RESULTS) {
        if (entry->d_name[0] == '.') continue;

        char full_path[WLT_FS_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Recurse into subdirectory (max depth 3) */
            if (strlen(dir) - strlen(q->search_dir) < 100)
                search_dir(full_path, q, ctx);
        } else if (S_ISREG(st.st_mode)) {
            /* Check extension */
            if (q->extension[0]) {
                const char *ext = strrchr(entry->d_name, '.');
                if (!ext || strcasecmp(ext + 1, q->extension) != 0)
                    continue;
            }
            /* Check time range */
            if (q->after && st.st_mtime < q->after) continue;
            if (q->before && st.st_mtime > q->before) continue;
            /* Check name contains */
            if (q->name_contains[0] && !strcasestr(entry->d_name, q->name_contains))
                continue;

            /* Match! Add to results */
            wlt_fs_result_t *r = &ctx->results[ctx->count];
            strncpy(r->path, full_path, WLT_FS_MAX_PATH - 1);
            strncpy(r->name, entry->d_name, sizeof(r->name) - 1);
            r->modified = st.st_mtime;
            r->size = st.st_size;
            ctx->count++;
        }
    }
    closedir(d);
}

wlt_ai_tool_result_t
wlt_ai_file_search(struct wlt_ai_ctx *ctx, const char *query)
{
    wlt_ai_tool_result_t result = {0};
    (void)ctx;
    if (!query)
        return result;

    char *results_out = result.output;
    size_t results_size = sizeof(result.output);

    wlt_fs_query_t fs_query;
    wlt_fs_search_ctx_t search_ctx;
    memset(&search_ctx, 0, sizeof(search_ctx));

    parse_query(query, &fs_query);
    search_dir(fs_query.search_dir, &fs_query, &search_ctx);

    /* Format results */
    size_t offset = 0;
    if (search_ctx.count == 0) {
        snprintf(results_out, results_size, "No files found matching '%s'", query);
    result.success = true;
    return result;
    }

    offset += snprintf(results_out + offset, results_size - offset,
                       "Found %zu file(s):\n", search_ctx.count);
    for (size_t i = 0; i < search_ctx.count && offset < results_size - 100; i++) {
        char timebuf[64];
        struct tm *tm = localtime(&search_ctx.results[i].modified);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm);
        offset += snprintf(results_out + offset, results_size - offset,
                           "  %s (%s, %zu bytes)\n",
                           search_ctx.results[i].path, timebuf, search_ctx.results[i].size);
    }

    result.success = true;
    return result;
}
