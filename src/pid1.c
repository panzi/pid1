#define _DEFAULT_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <spawn.h>
#include <signal.h>

void cleanup(void) {
    printf("sending SIGTERM to all processes\n");
    if (kill(-1, SIGTERM) != 0) {
        perror("sending SIGTERM to all processes");
    }

    printf("waiting for all processes\n");
    for (;;) {
        int status = 0;
        pid_t pid = wait(&status);

        if (pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno != ECHILD) {
                perror("*** error: waiting for processes");
            }
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
    bool intr = false;
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) {
            break;
        }

        if (pid < 1) {
            if (errno == EINTR) {
                if (intr) {
                    // if interrupted twice stop reaping
                    break;
                }
                intr = true;
                continue;
            }

            if (errno != ECHILD) {
                perror("*** error: waiting for processes");
            }
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
        fprintf(stderr, "*** error: forwardnng signal %d to pid %d: %s\n", sig, child_pid, strerror(errno));
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
        perror("*** error: installing zombie reaper");
        return 1;
    }

    if (signal(SIGTERM, forward_signal) == SIG_ERR) {
        perror("*** error: installing SIGTERM handler");
        return 1;
    }

    if (signal(SIGINT, forward_signal) == SIG_ERR) {
        perror("*** error: installing SIGINT handler");
        return 1;
    }

    if (atexit(cleanup) != 0) {
        perror("registering atexit handler");
        return 1;
    }

    int errnum = posix_spawnp(&child_pid, argv[1], NULL, NULL, &argv[1], environ);
    if (errnum != 0) {
        fprintf(stderr, "*** error: spawning %s: %s\n", argv[1], strerror(errnum));
        return 1;
    }

    for (;;) {
        int status = 0;
        pid_t result = waitpid(child_pid, &status, 0);
        if (result < 0) {
            if (errno != EINTR) {
                fprintf(stderr, "*** error: waiting for %s: %s", argv[1], strerror(errno));
                return 1;
            }
        } else {
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
    }
}
