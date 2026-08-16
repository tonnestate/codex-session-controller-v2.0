#define _GNU_SOURCE
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
        fprintf(stderr, "Spawns an interactive PTY and captures output to file.\n");
        return 1;
    }

    const char *output_file = argv[1];
    int master_fd, slave_fd, out_fd;
    pid_t child_pid;
    struct winsize ws;

    /* Initialize winsize to zero (FIX: was uninitialized) */
    memset(&ws, 0, sizeof(ws));

    /* Open PTY pair */
    if (openpty(&master_fd, &slave_fd, NULL, NULL, NULL) < 0) {
        perror("openpty");
        return 1;
    }

    /* Open output file safely: O_CREAT|O_WRONLY|O_TRUNC avoids symlink following */
    /* Mode 0600: read/write owner only (security) */
    out_fd = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (out_fd < 0) {
        perror("open");
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    /* Fork process */
    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        close(master_fd);
        close(slave_fd);
        close(out_fd);
        return 1;
    }

    if (child_pid == 0) {
        /* Child: set up slave side and run interactive command */
        close(master_fd);
        close(out_fd);

        if (setsid() < 0) {
            perror("setsid");
            exit(1);
        }

        /* Set slave as controlling terminal (FIX: was missing) */
        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            perror("ioctl TIOCSCTTY");
            exit(1);
        }

        if (ioctl(slave_fd, TIOCSWINSZ, &ws) < 0) {
            perror("ioctl TIOCSWINSZ");
            /* Non-fatal */
        }

        /* FIX: Check all dup2 calls */
        if (dup2(slave_fd, STDIN_FILENO) < 0 ||
            dup2(slave_fd, STDOUT_FILENO) < 0 ||
            dup2(slave_fd, STDERR_FILENO) < 0) {
            perror("dup2");
            exit(1);
        }

        if (slave_fd > 2) {
            close(slave_fd);
        }

        /* Run Codex in interactive mode */
        execl("/bin/bash", "bash", "-c", "codex result", (char *)NULL);
        perror("execl");
        exit(127);
    }

    /* Parent: read from master and write to output file */
    close(slave_fd);

    char buffer[4096];
    ssize_t n;
    while ((n = read(master_fd, buffer, sizeof(buffer))) > 0) {
        /* FIX: Check write() return value (was using fwrite before) */
        if (write(out_fd, buffer, n) != n) {
            perror("write");
            break;
        }
    }

    if (n < 0) {
        perror("read");
    }

    close(master_fd);
    close(out_fd);

    /* Wait for child to finish */
    int status;
    /* FIX: Check waitpid return value */
    if (waitpid(child_pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "Child killed by signal %d\n", WTERMSIG(status));
        return 1;
    }

    return 1;
}
