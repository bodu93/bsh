#ifndef BDU_SHELL_PARSE_H
#define BDU_SHELL_PARSE_H

#include "util.h"

#define ARGSMAXCOUNT        20      /* single command max args count */
#define MAXPIPECOUNT        10      /* max pipe count */
#define MAXCMDLINE          4096

typedef struct command_frag command_frag;
struct command_frag {
    struct string_view stdinfile;
    struct string_view stdoutfile;
    int                stdoutfile_openflag;
    struct string_view stderrfile;
    int                stderrfile_openflag;
    int                stderr_to_stdout_flag;
    struct string_view arguments[ARGSMAXCOUNT];
};

typedef struct pipe_command pipe_command;
struct pipe_command {
    char* arglist[ARGSMAXCOUNT + 1];
    int   stdinfd;
    int   stdoutfd;
    int   stderrfd;
};

void parse_error(char ch);
size_t next_arg(const char* cmd, size_t cmdlen, struct string_view* sv);
int parse_command_no_pipe(const struct string_view* cmdfrag,
                          int input_max,
                          int output_max,
                          struct command_frag* cmdref);
int parse_command_with_pipe(const char* cmd,
                            size_t cmdlen,
                            struct command_frag* fragarray);
int parse_and_execute_cmdline(const char* cmdline);

#endif /* BDU_SHELL_PARSE_H */
