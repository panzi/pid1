#define _DEFAULT_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <spawn.h>
#include <signal.h>

void cleanup() {
    printf("sending SIGTERM to all processes\n");
    if (kill(-1, SIGTERM) != 0) {
        perror("sending SIGTERM to all processes");
    }

    printf("waiting for all processes\n");
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid < 1) {
            break;
        }

        if (WIFSIGNALED(status)) {
            printf("pid %d exited with signal %u\n", pid, WTERMSIG(status));
        } else {
            printf("pid %d exited with status %u\n", pid, WEXITSTATUS(status));
        }
    }
}

void reaper(int sig) {
    size_t count = 0;
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid < 1) {
            break;
        }

        ++ count;
    }

    if (count == 1) {
        printf("reaped 1 zombie\n");
    } else {
        printf("reaped %zu zombies\n", count);
    }
}

extern char **environ;

pid_t child_pid = 0;

void forward_signal(int sig) {
    if (child_pid != 0 && kill(child_pid, sig) != 0) {
        fprintf(stderr, "error forwardning signal %d to pid %d: %s\n", sig, child_pid, strerror(errno));
    }
}

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

    if (signal(SIGTERM, forward_signal) == SIG_ERR) {
        perror("installing SIGTERM handler");
        return 1;
    }

    if (signal(SIGINT, forward_signal) == SIG_ERR) {
        perror("installing SIGINT handler");
        return 1;
    }

    if (atexit(cleanup) != 0) {
        perror("registering atexit handler");
        return 1;
    }

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
