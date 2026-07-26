/**
 * @file wlt-taskmgr.c — WLTIOS Task Manager (FR-APP-05)
 * @purpose Real process list from /proc, not placeholder data.
 *          Same discipline as the statusbar's real data.
 * @build: gcc -O2 -Wall -o wlt-taskmgr wlt-taskmgr.c
 * @usage: wlt-taskmgr [--watch]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

typedef struct {
    int pid;
    char comm[256];
    char state;
    unsigned long vsize;  /* virtual memory in KB */
    unsigned long rss;    /* resident set size in KB */
    unsigned long utime;  /* user time in clock ticks */
    unsigned long stime;  /* system time in clock ticks */
} proc_info_t;

static int read_proc(int pid, proc_info_t *info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    /* /proc/PID/stat format (first few fields):
     * pid (comm) state ppid pgrp session tty_nr tpgid flags ... */
    char comm[256] = {0};
    char state;
    /* Read PID + comm (in parens) + state */
    if (fscanf(f, "%d (%255[^)]) %c", &info->pid, comm, &state) != 3) {
        fclose(f); return -1;
    }
    strncpy(info->comm, comm, sizeof(info->comm)-1);
    info->state = state;
    /* Skip to field 23 (vsize) and 24 (rss) — count by reading */
    /* The stat file has many fields; we skip to vsize (field 23) */
    long val;
    for (int i = 4; i <= 22; i++) {
        if (fscanf(f, "%ld", &val) != 1) { fclose(f); return -1; }
    }
    /* field 23 = vsize (bytes), field 24 = rss (pages) */
    unsigned long vsize, rss;
    if (fscanf(f, "%lu %lu", &vsize, &rss) != 2) { fclose(f); return -1; }
    info->vsize = vsize / 1024;  /* convert to KB */
    info->rss = rss * (getpagesize() / 1024);  /* pages to KB */
    /* fields 14+15 = utime, stime */
    for (int i = 25; i <= 27; i++) {
        if (fscanf(f, "%ld", &val) != 1) { fclose(f); return -1; }
    }
    unsigned long utime, stime;
    if (fscanf(f, "%lu %lu", &utime, &stime) != 2) { fclose(f); return -1; }
    info->utime = utime;
    info->stime = stime;
    fclose(f);
    return 0;
}

static int cmp_vsize(const void *a, const void *b) {
    const proc_info_t *pa = a, *pb = b;
    if (pb->vsize > pa->vsize) return 1;
    if (pb->vsize < pa->vsize) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    int watch = 0;
    if (argc > 1 && strcmp(argv[1], "--watch") == 0) watch = 1;

    do {
        DIR *d = opendir("/proc");
        if (!d) { perror("opendir /proc"); return 1; }

        proc_info_t procs[512];
        int count = 0;
        struct dirent *de;
        while ((de = readdir(d)) != NULL && count < 512) {
            /* Only numeric directory names are PIDs */
            if (!isdigit(de->d_name[0])) continue;
            int pid = atoi(de->d_name);
            if (pid <= 0) continue;
            if (read_proc(pid, &procs[count]) == 0) count++;
        }
        closedir(d);

        /* Sort by memory usage (descending) */
        qsort(procs, count, sizeof(proc_info_t), cmp_vsize);

        /* Clear screen (if watching) */
        if (watch) printf("\033[2J\033[H");
        printf("=== WLTIOS Task Manager ===\n");
        printf("%-8s %-20s %4s %10s %10s %8s\n",
               "PID", "COMMAND", "ST", "VSZ(KB)", "RSS(KB)", "CPU%");
        printf("%-8s %-20s %4s %10s %10s %8s\n",
               "-------", "--------------------", "--", "----------", "----------", "-------");

        for (int i = 0; i < count && i < 30; i++) {  /* top 30 */
            /* CPU% = (utime + stime) / CLK_TCK — simplified (raw ticks) */
            double cpu = (procs[i].utime + procs[i].stime) / 100.0;
            printf("%-8d %-20.20s %4c %10lu %10lu %7.1f\n",
                   procs[i].pid, procs[i].comm, procs[i].state,
                   procs[i].vsize, procs[i].rss, cpu);
        }
        printf("\nTotal: %d processes (showing top 30 by memory)\n", count);

        if (watch) {
            printf("(auto-refresh — Ctrl+C to stop)\n");
            sleep(2);
        }
    } while (watch);

    return 0;
}
