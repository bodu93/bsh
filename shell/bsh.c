#include "bsh.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static const char* PROMPT       = "bsh> ";

/*
 * built-in commands
 */
enum builtins {
    eCD = 1,
    eEXIT
};

void do_redirection(int stdinfd, int stdoutfd, int stderrfd) {
    if (stdinfd >= 0) {
        if (dup2(stdinfd, STDIN_FILENO) < 0) {
            fprintf(stderr, "bsh: dup2 STDIN_FILENO failed %s\n",
                    strerror(errno));
            return;
        }
        /* ignore fail of close call
         * when process terminated,
         * all opened file descriptors will closed forcedly
         */
        close(stdinfd);
    }

    if (stdoutfd >= 0) {
        if (dup2(stdoutfd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "bsh: dup2 STDOUT_FILENO failed %s\n",
                    strerror(errno));
            return;
        }
        close(stdoutfd);
    }

    if (stderrfd >= 0) {
        if (dup2(stderrfd, STDERR_FILENO) < 0) {
            fprintf(stderr, "bsh: dup2 STDERR_FILENO failed %s\n",
                    strerror(errno));
            return;
        }
        close(stderrfd);
    }
}

int execute(const struct pipe_command* command) {
    assert(command &&
           command->arglist != NULL &&
           command->arglist[0] != NULL);

    do_redirection(command->stdinfd, command->stdoutfd, command->stderrfd);
    execvp(command->arglist[0], command->arglist);
    return -1;
}

pid_t fork_and_execute(const struct pipe_command* command) {
    assert(command);

    pid_t pid;
    if ((pid = fork()) < 0) {
        fprintf(stderr, "bsh: fork error for %s.\n", strerror(errno));
        return -2; /* fork failed */
    } else if (pid == 0) {
        restore_signals();

        execute(command);
        fprintf(stderr, "bsh: execute %s error.\n", command->arglist[0]);
        /* if support 'echo $?' command,
         * may be should make exit value more clear... */
        exit(-1);
    }
    return pid;
}

pid_t execute_with_pipe(struct pipe_command** pipe_commands,
                      size_t pipe_commands_len) {
    assert(pipe_commands && pipe_commands[0] != NULL);

    pid_t child_pid;
    int readfd = -1;

    pid_t grand_child_pid;
    if ((child_pid = fork()) < 0) {
        return -1;
    } else if (child_pid == 0) {
        /* create a new process group */
        /* setpgid(0, 0); */

        /* execute pipe commands in grand-child process */
        int first_pipefd[2];
        pipe(first_pipefd);
        pipe_commands[0]->stdoutfd = first_pipefd[1];
        /* execute first command */
        grand_child_pid = fork_and_execute(pipe_commands[0]);
        close(first_pipefd[1]);
        if (grand_child_pid < 0 ) {
            fprintf(stderr, "bsh: fork error.\n");
            exit(-1);
        }
        readfd = first_pipefd[0];

        for (size_t i = 1; i < pipe_commands_len - 1; i++) {
            int pipefd[2];
            pipe(pipefd);
            pipe_commands[i]->stdinfd = readfd;
            pipe_commands[i]->stdoutfd = pipefd[1];

            grand_child_pid = fork_and_execute(pipe_commands[i]);
            close(readfd);
            readfd = pipefd[0];
            close(pipefd[1]);
            if (grand_child_pid < 0) {
                fprintf(stderr, "bsh: fork error.\n");
                exit(-1);
            }
        }

        pipe_commands[pipe_commands_len - 1]->stdinfd = readfd;
        /* execute last command in child process */
        grand_child_pid = execute(pipe_commands[pipe_commands_len - 1]);
        close(readfd);
        if (grand_child_pid < 0) {
            fprintf(stderr, "bsh: fork error.\n");
            exit(-1);
        }
    }

    return child_pid;
}

int is_builtins(struct pipe_command** pipe_commands) {
    assert(pipe_commands && pipe_commands[0] != NULL);
    assert(pipe_commands[0]->arglist != NULL && pipe_commands[0]->arglist[0] != NULL);

    char* command = pipe_commands[0]->arglist[0];
    assert(command != NULL);
    
    /* only support two built-in commands: cd, exit... */
    if (strcmp(command, "cd") == 0) {
        return eCD;
    } else if (strcmp(command, "exit") == 0) {
        return eEXIT;
    } else {
        return 0;
    }

    return 0;
}

int do_builtins(struct pipe_command** pipe_commands) {
    char** arglist = pipe_commands[0]->arglist;

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) { /* cd to home dir */
            char* homedir = getenv("HOME");
            if (homedir) {
                if (chdir(homedir) < 0) {
                    fprintf(stderr, "cd: error for %s.\n", strerror(errno));
                    return -1;
                }
            } else {
                fprintf(stderr, "cd: unknown home dir.\n");
                return -1;
            }
        } else {
            char* todir = arglist[1];
            if (chdir(todir) < 0) {
                fprintf(stderr, "cd: error for %s.\n", strerror(errno));
                return -1;
            }
        }
    } else if (strcmp(arglist[0], "exit") == 0) {
        /* indicate exit loop... */
        return -2;
    }

    return 0;
}

int execute_command(struct pipe_command** pipe_commands, size_t commands_len) {
    assert(pipe_commands != NULL && pipe_commands[0] != NULL);

    int built_in = is_builtins(pipe_commands);
    if (built_in) {
        return do_builtins(pipe_commands);
    }

    pid_t child_pid;
    if (commands_len == 1) { /* no pipe */
        child_pid = fork_and_execute(*pipe_commands);
    } else {
        child_pid = execute_with_pipe(pipe_commands, commands_len);
    }

    if (child_pid < 0) return (int)child_pid;
    waitpid(child_pid, NULL, 0);

    return 0;
}

void ignore_signals() {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

void restore_signals() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
}

    

int main() {
    ignore_signals();

    char cmdline[MAXCMDLINE + 1];
    cmdline[0] = 0;

    while (1) {
        printf("%s", PROMPT);
        fflush(NULL);

        char* input = fgets(cmdline, MAXCMDLINE + 1, stdin);
        if (input != NULL) {
            size_t cmdlinelen = strlen(input);
            if (cmdlinelen == MAXCMDLINE && cmdline[MAXCMDLINE - 1] != '\n') {
                printf("\n\tinput line exceed max count %d\n", MAXCMDLINE);
            } else {
                cmdline[cmdlinelen - 1] = '\0';
                int err = parse_and_execute_cmdline(cmdline);
                if (err == -2) {
                    fprintf(stdout, "Bye......\n");
                    break;
                }
            }
        } else {
            /*
             * call this function clear EOF tag,
             * otherwise, we will get in a dead loop:)
             */
            clearerr(stdin);
            fprintf(stdout, "\n");
        }
    }

    exit(0);
}
