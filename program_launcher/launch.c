#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable> [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child */
        if (raise(SIGSTOP) != 0) {
            perror("raise(SIGSTOP)");
            _exit(1);
        }

        execv(argv[1], &argv[1]);
        perror("execv");
        _exit(1);
    }

    /* Parent */
    int status;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
        return 1;
    }

    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "Child did not stop as expected\n");
        return 1;
    }

    printf("Child created and stopped.\n");
    printf("PID = %d\n", pid);
    printf("Start the module now, then press ENTER to start execution...\n");

    getchar();

    if (kill(pid, SIGCONT) < 0) {
        perror("kill(SIGCONT)");
        return 1;
    }

    printf("Child resumed.\n");

    /* Optional: wait for child to exit */
    waitpid(pid, NULL, 0);

    printf("Child finished.\n");

    return 0;
}
