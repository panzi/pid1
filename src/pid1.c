#define _DEFAULT_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <spawn.h>
#include <signal.h>

pid_t parse_piddir(const char *filename) {
    char *endptr = NULL;
    long pid = strtol(filename, &endptr, 10);
    if (*endptr) {
        return -1;
    }
    return pid;
}

void cleanup() {
    printf("sending SIGTERM to all processes\n");

    DIR *dir = opendir("/proc");

    if (dir == NULL) {
        perror("/proc");
        return;
    }

    for (;;) {
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            break;
        }

        if (entry->d_type == DT_DIR) {
            pid_t pid = parse_piddir(entry->d_name);

            if (pid > 1) {
                if (kill(pid, SIGTERM) != 0) {
                    perror(entry->d_name);
                }
            }
        }
    }

    closedir(dir);

    printf("waiting for all processes\n");
    for (;;) {
        size_t proc_count = 0;
        DIR *dir = opendir("/proc");

        if (dir == NULL) {
            perror("/proc");
            break;
        }

        for (;;) {
            struct dirent *entry = readdir(dir);
            if (entry == NULL) {
                break;
            }

            if (entry->d_type == DT_DIR) {
                pid_t pid = parse_piddir(entry->d_name);

                if (pid > 1) {
                    int status = 0;
                    pid_t result = waitpid(pid, &status, 0);
                    if (result < 0) {
                        ++ proc_count;
                        fprintf(stderr, "error waiting for PID %d: %s\n", pid, strerror(errno));
                    }
                }
            }
        }

        closedir(dir);

        if (proc_count == 0) {
            break;
        }
    }
}

void reaper(int sig) {
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid < 1) {
            break;
        }
    }
}

extern char **environ;

int main(int argc, char *argv[]) {
    if (getpid() != 1) {
        fprintf(stderr, "pid1 needs to be run with process ID 1\n");
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "no child command to run specified\n");
        return 1;
    }

    if (signal(SIGCHLD, reaper) == SIG_ERR) {
        perror("installing zombie reaper");
        return 1;
    }

    if (atexit(cleanup) != 0) {
        perror("registering atexit handler");
        return 1;
    }

    pid_t child_pid = 0;
    int errnum = posix_spawnp(&child_pid, argv[1], NULL, NULL, &argv[1], environ);
    if (errnum != 0) {
        fprintf(stderr, "error spawning %s: %s\n", argv[1], strerror(errnum));
        return 1;
    }

    int status = 0;
    pid_t result = waitpid(child_pid, &status, 0);
    if (result < 0) {
        fprintf(stderr, "error waiting for %s: %s", argv[1], strerror(errno));
        return 1;
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s exited with signal %u\n", argv[1], WTERMSIG(status));
        return 1;
    }

    int exit_status = WEXITSTATUS(status);
    if (exit_status != 0) {
        fprintf(stderr, "%s exited abnormally with status: %d\n", argv[1], exit_status);
    } else {
        printf("%s exited normally\n", argv[1]);
    }

    return exit_status;
}
