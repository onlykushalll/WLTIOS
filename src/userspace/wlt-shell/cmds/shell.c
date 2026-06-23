/**
 * @file shell.c
 * @brief Basic shell — command interpreter for WLTIOS
 * @module wlt-shell/shell
 * @purpose Read commands, parse, execute builtins + external commands
 * @dependencies: none
 * @tested_by: tests/userspace/shell_test.c
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define WLT_SHELL_MAX_INPUT 1024
#define WLT_SHELL_MAX_ARGS 64
#define WLT_SHELL_MAX_PATH 4096
#define WLT_SHELL_PROMPT "/wlt# "

static char g_cwd[WLT_SHELL_MAX_PATH];

/* ─── Built-in commands ──────────────────────────────── */

static int
cmd_ls(int argc, char **argv)
{
    bool show_all = false;
    bool long_format = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (char *p = argv[i] + 1; *p; p++)
            {
                if (*p == 'a') show_all = true;
                if (*p == 'l') long_format = true;
            }
        }
    }

    const char *path = g_cwd;
    if (argc > 1 && argv[1][0] != '-')
        path = argv[1];
    DIR *d = opendir(path);
    if (d == NULL)
    {
        perror("ls");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (!show_all && entry->d_name[0] == '.')
            continue;

        if (long_format)
        {
            char full_path[WLT_SHELL_MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0)
            {
                char type = S_ISDIR(st.st_mode) ? 'd' : '-';
                printf("%c%c%c%c%c%c%c%c%c%c %8lu %s\n",
                       type,
                       (st.st_mode & S_IRUSR) ? 'r' : '-',
                       (st.st_mode & S_IWUSR) ? 'w' : '-',
                       (st.st_mode & S_IXUSR) ? 'x' : '-',
                       (st.st_mode & S_IRGRP) ? 'r' : '-',
                       (st.st_mode & S_IWGRP) ? 'w' : '-',
                       (st.st_mode & S_IXGRP) ? 'x' : '-',
                       (st.st_mode & S_IROTH) ? 'r' : '-',
                       (st.st_mode & S_IWOTH) ? 'w' : '-',
                       (st.st_mode & S_IXOTH) ? 'x' : '-',
                       (unsigned long)st.st_size,
                       entry->d_name);
            }
        }
        else
        {
            printf("%s  ", entry->d_name);
        }
    }

    if (!long_format)
        printf("\n");

    closedir(d);
    return 0;
}

static int
cmd_cd(int argc, char **argv)
{
    const char *target = (argc > 1) ? argv[1] : getenv("HOME");
    if (target == NULL)
        target = "/";

    char new_path[WLT_SHELL_MAX_PATH];

    if (target[0] == '/')
    {
        strncpy(new_path, target, sizeof(new_path) - 1);
    }
    else
    {
        snprintf(new_path, sizeof(new_path), "%s/%s", g_cwd, target);
    }

    /* Resolve path */
    char resolved[WLT_SHELL_MAX_PATH];
    if (realpath(new_path, resolved) == NULL)
    {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "cd: %s: Not a directory\n", target);
        return 1;
    }

    strncpy(g_cwd, resolved, sizeof(g_cwd) - 1);
    g_cwd[sizeof(g_cwd) - 1] = '\0';
    setenv("PWD", g_cwd, 1);

    return 0;
}

static int
cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        /* Read from stdin */
        char buf[4096];
        ssize_t n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, n);
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        FILE *f = fopen(argv[i], "r");
        if (f == NULL)
        {
            fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
            return 1;
        }

        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            fwrite(buf, 1, n, stdout);

        fclose(f);
    }

    return 0;
}

static int
cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}

static int
cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (mkdir(argv[i], 0755) != 0 && errno != EEXIST)
        {
            fprintf(stderr, "mkdir: %s: %s\n", argv[i], strerror(errno));
            return 1;
        }
    }
    return 0;
}

static int
cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }

    bool recursive = false;
    bool force = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (char *p = argv[i] + 1; *p; p++)
            {
                if (*p == 'r') recursive = true;
                if (*p == 'f') force = true;
            }
            continue;
        }

        if (unlink(argv[i]) != 0)
        {
            if (errno == EISDIR && recursive)
            {
                /* TODO: recursive removal */
                fprintf(stderr, "rm: -r not yet implemented\n");
                return 1;
            }
            if (!force)
                fprintf(stderr, "rm: %s: %s\n", argv[i], strerror(errno));
        }
    }

    return 0;
}

static int
cmd_pwd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("%s\n", g_cwd);
    return 0;
}

static int
cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("WLTIOS Shell — Built-in commands:\n");
    printf("  ls [-al]        List directory contents\n");
    printf("  cd [dir]        Change directory\n");
    printf("  cat [file...]   Display file contents\n");
    printf("  echo [text...]  Print text\n");
    printf("  mkdir [dir...]  Create directories\n");
    printf("  rm [-rf] [file] Remove files\n");
    printf("  pwd             Print working directory\n");
    printf("  help            Show this help\n");
    printf("  exit            Exit shell\n");
    return 0;
}

/* ─── Command table ──────────────────────────────────── */

typedef struct {
    const char *name;
    int (*func)(int argc, char **argv);
} wlt_shell_command_t;

static wlt_shell_command_t commands[] = {
    {"ls",    cmd_ls},
    {"cd",    cmd_cd},
    {"cat",   cmd_cat},
    {"echo",  cmd_echo},
    {"mkdir", cmd_mkdir},
    {"rm",    cmd_rm},
    {"pwd",   cmd_pwd},
    {"help",  cmd_help},
    {NULL, NULL},
};

/* ─── Input parsing ──────────────────────────────────── */

static int
parse_input(char *input, char **argv, int max_args)
{
    int argc = 0;
    char *p = input;

    while (*p && argc < max_args - 1)
    {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0' || *p == '\n')
            break;

        /* Handle quoted strings */
        if (*p == '"')
        {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"' && *p != '\n')
                p++;
            if (*p == '"')
                *p++ = '\0';
        }
        else
        {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n')
                p++;
        }

        if (*p)
            *p++ = '\0';
    }

    argv[argc] = NULL;
    return argc;
}

/* ─── Main shell loop ────────────────────────────────── */

int
main(int argc, char **argv)
{
 (void)argc;
 (void)argv;

 /* Disable stdio buffering — fixes null bytes in QEMU serial */
 setvbuf(stdout, NULL, _IONBF, 0);
 setvbuf(stderr, NULL, _IONBF, 0);

 /* Initialize CWD */
 if (getcwd(g_cwd, sizeof(g_cwd)) == NULL)
  strncpy(g_cwd, "/", sizeof(g_cwd) - 1);

 char input[WLT_SHELL_MAX_INPUT];

 printf("WLTIOS Shell v1.0\n");
 printf("Type 'help' for available commands.\n\n");

 while (1)
 {
  if (strcmp(g_cwd, "/") == 0)
   printf("/wlt# ");
  else
   printf("%s/wlt# ", g_cwd);
  fflush(stdout);

  if (fgets(input, sizeof(input), stdin) == NULL)
  {
   printf("\n");
   break;
  }

  if (input[0] == '\n')
   continue;

  char *cmd_argv[WLT_SHELL_MAX_ARGS];
  int cmd_argc = parse_input(input, cmd_argv, WLT_SHELL_MAX_ARGS);

  if (cmd_argc == 0)
   continue;

  if (strcmp(cmd_argv[0], "exit") == 0)
   break;

  /* Check for output redirection */
  int saved_stdout = -1;
  for (int ri = 0; ri < cmd_argc; ri++) {
   if (cmd_argv[ri] && (strcmp(cmd_argv[ri], ">") == 0 || strcmp(cmd_argv[ri], ">>") == 0)) {
    if (ri + 1 < cmd_argc) {
     int append = (cmd_argv[ri][1] == '>');
     int fd = open(cmd_argv[ri + 1], O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
     if (fd >= 0) {
      saved_stdout = dup(STDOUT_FILENO);
      dup2(fd, STDOUT_FILENO);
      close(fd);
      cmd_argv[ri] = NULL;
      cmd_argc = ri;
     }
    }
   }
  }

  /* Check built-in commands */
  bool found = false;
  for (int i = 0; commands[i].name != NULL; i++)
  {
   if (strcmp(cmd_argv[0], commands[i].name) == 0)
   {
    commands[i].func(cmd_argc, cmd_argv);
    found = true;
    break;
   }
  }

  /* If not built-in, try external command */
  if (!found)
  {
   pid_t pid = fork();
   if (pid == 0)
   {
    execvp(cmd_argv[0], cmd_argv);
    fprintf(stderr, "%s: command not found\n", cmd_argv[0]);
    _exit(127);
   }
   else if (pid > 0)
   {
    int status;
    waitpid(pid, &status, 0);
   }
   else
   {
    perror("fork");
   }
  }

  /* Restore stdout if redirected */
  if (saved_stdout >= 0) {
   dup2(saved_stdout, STDOUT_FILENO);
   close(saved_stdout);
  }
 }

 return 0;
}
