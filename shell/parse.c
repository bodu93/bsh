#include "parse.h"
#include "bsh.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

static const char SPLITSIGN     = ';';

/*
 * stdout or stderr redirection cases:
 * >stdout_file
 * >>stdout_file
 * 2>stdout_file
 * 2>>stdout_file
 */
enum output_redirection_cases {
    WRONLY,
    APPEND,
    ERR_WRONLY,
    ERR_APPEND
};

void parse_error(char ch) {
    fprintf(stderr, "bsh: parse error near '%c'.\n", ch);
}

size_t next_arg_len(const char* cmd, size_t cmdlen) {
    size_t rank = 0;
    while (rank < cmdlen) {
        char ch = cmd[rank];
        if (ch == '\\') {
            if (rank + 1 < cmdlen &&
                (cmd[rank + 1] == '<' ||
                 cmd[rank + 1] == '>')) {
                rank += 1;
            }
        } else if (ch == ' '  ||
                   ch == '\t' ||
                   ch == '<'  ||
                   ch == '>') {
            break;
        }
        ++rank;
    }

    return rank;
}

size_t next_arg(const char* cmd, size_t cmdlen, struct string_view* sv) {
    assert(sv != NULL);
    
    size_t endofwhitespaces = skip_whitespaces(cmd, cmdlen);
    size_t argbegin = endofwhitespaces;
    size_t arglen   = next_arg_len(cmd + argbegin, cmdlen - argbegin);
    if (arglen > 0) {
        sv->str = cmd + argbegin;
        sv->len = arglen;
    }
    /*
     * if arglen equals 0,
     * after blanks(if exists), cmd[rank] is '<' or '>'
     * indicate a parse error
     */

    return endofwhitespaces + arglen;
}

int parse_command_no_pipe(const struct string_view* cmdfrag,
                          int input_max,
                          int output_max,
                          struct command_frag* cmdref) {
    assert(cmdfrag && cmdref);

    /* redirection count */
    int input_count = 0;
    struct string_view input_file;
    int output_count = 0;
    struct string_view output_file;
    int stdout_open_flag = 0;
    int stderr_open_flag = 0;
    int err_max = 1;
    int err_count = 0;
    int stderr_to_stdout_flag = 0;
    struct string_view err_file;

    /* single-command arguments */
    struct string_view argfrags[ARGSMAXCOUNT + 1];
    size_t argrank = 0;

    size_t rank = 0;
    const char* cmd = cmdfrag->str;
    const size_t cmdlen = cmdfrag->len;
    while (rank < cmdlen) {
        switch (cmd[rank]) {
            case ' ':
            case '\t':
                ++rank;
                break;

            case '\\':
                {
                    struct string_view sv;
                    sv.str = cmd + rank;

                    rank += 1;
                    int slashlen = 1;
                    if (rank < cmdlen &&
                        (cmd[rank] == '<' ||
                         cmd[rank] == '>')) {
                        rank += 1;
                        slashlen += 1;
                    }
                    sv.len = slashlen;

                    struct string_view next_sv;
                    next_sv.len = 0;
                    size_t arglen = next_arg(cmd + rank, cmdlen - rank, &next_sv);
                    if (arglen > 0 && arglen == next_sv.len) { /* no space or end */
                        rank += arglen;
                        sv.len += arglen;
                    }
                    argfrags[argrank++] = sv;
                    if (argrank == ARGSMAXCOUNT + 1) {
                        fprintf(stderr, "bsh: too much arguments.\n");
                        return -1;
                    }
                }
                break;

            case '<':
                    /*
                     * stdin redirection cases(at most 1):
                     * < in_file
                     * <in_file
                     *
                     */
                {
                    rank += 1;
                    struct string_view sv;
                    size_t len = next_arg(cmd + rank, cmdlen - rank, &sv);
                    if (len > 0 && skip_whitespaces(sv.str, len) < len && input_count < input_max) {
                        input_file = sv;
                        input_count += 1;
                    } else {
                        parse_error('<');
                        return -1;
                    }
                    rank += len;
                }
                break;


            case '>':
                {
                    /*                   
                     * stdout redirection cases(at most 1):
                     * > out_file
                     * >out_file
                     * >> out_file
                     * >>out_file
                     *
                     * stderr redirection cases:
                     * 2>&1
                     * 2> filename
                     * 2>filename
                     * 2>> filename
                     * 2>>filename
                     */
                    int output_redirection_case = WRONLY; /* stdout redirection */
                    if (rank > 0 &&
                        cmd[rank - 1] == '2' &&
                        (rank - 1 == 0 || isblank(cmd[rank - 2]))) {
                        /*
                         * 2>err_file
                         * ^^^^
                         *  r
                         */
                        output_redirection_case = ERR_WRONLY; /* stderr redirection */
                    }

                    if (rank + 1 < cmdlen && cmd[rank + 1] == '>') {
                        if (output_redirection_case == WRONLY) {
                            output_redirection_case = APPEND; /* append stdout redirection */
                        } else { /* 2>>.. */
                            output_redirection_case = ERR_APPEND; /* append stderr redirection */
                        }
                        rank += 2;
                    } else {
                        rank += 1;
                    }

                    struct string_view sv;
                    size_t len = next_arg(cmd + rank, cmdlen - rank, &sv);
                    if (len > 0 && skip_whitespaces(cmd + rank, len) < len) { /* next argument is not empty */
                        switch (output_redirection_case) {
                            case WRONLY: /* create write */
                            case APPEND: /* append */
                                if (output_count < output_max) {
                                    output_file = sv;
                                    output_count += 1;
                                } else {
                                    parse_error('>');
                                    return -1;
                                }
                                break;

                            case ERR_WRONLY: /* stderr redirection */
                            case ERR_APPEND:
                                /* no blanks in 2>&1 */
                                if (len == sv.len && sv.len == 2 && strncmp(sv.str, "&1", 2) == 0) {
                                    stderr_to_stdout_flag = 1;
                                    output_redirection_case = ERR_WRONLY; /* support 2>>&1 */
                                }

                                if (err_count < err_max) {
                                    /* go back 1 */
                                    assert(argrank > 0);
                                    argrank -= 1;

                                    err_count += 1;
                                    if (!stderr_to_stdout_flag)
                                        err_file = sv;
                                } else {
                                    fprintf(stderr, "bsh: allow only at most 1 stderr redirection.\n");
                                    return -1;
                                }
                                break;

                            default:
                                assert(0 && "invalid output redirection case.");
                        }

                        switch (output_redirection_case) {
                            case WRONLY:
                                stdout_open_flag = O_WRONLY | O_CREAT;
                                break;

                            case ERR_WRONLY:
                                stderr_open_flag = O_WRONLY | O_CREAT;
                                break;

                            case APPEND:
                                stdout_open_flag = O_APPEND | O_CREAT;
                                break;

                            case ERR_APPEND:
                                stderr_open_flag = O_APPEND | O_CREAT;
                                break;

                            default:
                                assert(0 && "unreachable switch-case");
                                break;
                        }
                    } else {
                        fprintf(stderr, "bsh: parse error near '>', empty input file.\n");
                        return -1;
                    }

                    rank += len;
                }
                break;

            default:
                {
                    struct string_view sv;
                    size_t arglen = next_arg(cmd + rank, cmdlen - rank, &sv);
                    assert(arglen > 0);
                    argfrags[argrank++] = sv;
                    if (argrank == ARGSMAXCOUNT + 1) {
                        fprintf(stderr, "bsh: too much arguments.\n");
                        return -1;
                    }
                    rank += arglen;
                }
                break;
        }
    }
    /* last argument is empty */
    argfrags[argrank].str = NULL;
    argfrags[argrank].len = 0;

    if (input_count) cmdref->stdinfile = input_file;
    if (output_count) {
        cmdref->stdoutfile = output_file;
        cmdref->stdoutfile_openflag = stdout_open_flag;
    }
    if (err_count) {
        cmdref->stderrfile = err_file;
        cmdref->stderrfile_openflag = stderr_open_flag;
    }
    cmdref->stderr_to_stdout_flag = stderr_to_stdout_flag;
    for (size_t i = 0; i < argrank; i++) {
        cmdref->arguments[i].str = argfrags[i].str;
        cmdref->arguments[i].len = argfrags[i].len;
    }

    return 0;
}

int parse_command_with_pipe(const char* cmd,
                            size_t cmdlen,
                            struct command_frag* pipe_commands) {
    assert(cmd);

    struct string_view pipefrags[MAXPIPECOUNT + 1];
    size_t piperank = 0;

    size_t rank = 0;
    size_t fragbegin = rank;
    while (rank < cmdlen) {
        char ch = cmd[rank];
        if (ch == '|' && piperank == 0 && skip_whitespaces(cmd, rank) == rank) {
            /* all leading blanks */
            fprintf(stderr, "bsh: lead pipe\n");
            return -1;
        } else if (ch == '|' &&
                   rank > 0  &&
                   cmd[rank - 1] != '\\') {
            size_t fraglen = rank - fragbegin;
            size_t wslen = skip_whitespaces(cmd + fragbegin, fraglen);
            if (fraglen > 0 && wslen < fraglen) {
                struct string_view sv;
                sv.str = cmd + fragbegin;
                sv.len = fraglen;
                pipefrags[piperank++] = sv;
                if (piperank >= MAXPIPECOUNT + 1) {
                    fprintf(stderr, "bsh: too much pipes\n");
                    return -1;
                }
                fragbegin = rank + 1;
            } else {
                parse_error('|');
                return -1;
            }
        }

        ++rank;
    }

    /* last frag */
    assert(rank == cmdlen);
    size_t fraglen = rank - fragbegin;
    size_t wslen = skip_whitespaces(cmd + fragbegin, fraglen);
    if (fraglen > 0 && wslen < fraglen) {
        struct string_view sv;
        sv.str = cmd + fragbegin;
        sv.len = fraglen;
        pipefrags[piperank++] = sv;
        if (piperank >= MAXPIPECOUNT + 1) {
            fprintf(stderr, "bsh: too much pipes.\n");
            return -1;
        }
    } else {
        fprintf(stderr, "bsh: tail pipe sign\n");
        return -1;
    }

    /* if all parsing correct, fill parsing results */
    size_t i = 1;
    if (piperank == 1) { /* no pipe */
        if (parse_command_no_pipe(&pipefrags[0], 1, 1, pipe_commands + 0) < 0)
            return -1;
    } else {
        for (i = 0; i < piperank; i++) {
            if (i == 0) {
                if (parse_command_no_pipe(&pipefrags[i], 1, 0, pipe_commands + i) < 0) {
                    return -1;
                }
            } else if (i == piperank - 1) {
                if (parse_command_no_pipe(&pipefrags[i], 0, 1, pipe_commands + i) < 0) {
                    return -1;
                }
            } else {
                if (parse_command_no_pipe(&pipefrags[i], 0, 0, pipe_commands + i) < 0) {
                    return -1;
                }
            }
        }
    }
    /* indicate pipe commands end */
    pipe_commands[i].stderr_to_stdout_flag = -1;

    return 0;
}

int open_file(const struct string_view* file_sv, int openflag) {
    assert(file_sv);

    /*
     * return -2, just make difference with the wrong return value
     * of system call open(2)
     */
    if (file_sv->str == NULL) return -2;

    char buf[1024];
    snprintf(buf, file_sv->len + 1, "%s", file_sv->str);
    /* keep safe */
    buf[1023] = 0;

    int fd = -1;
    if (openflag & O_CREAT) {
        fd = open(buf, openflag, 0666);
    } else {
        fd = open(buf, openflag);
    }
    /* handle open error */
    if (fd < 0) {
        fprintf(stderr, "bsh: open %s failed for %s.\n", buf, strerror(errno));
    }
    return fd;
}

struct pipe_command* mk_pipecommand(const struct command_frag* cmdfrag) {
    assert(cmdfrag);

    struct pipe_command* pcmd =
        (struct pipe_command*)malloc(sizeof(struct pipe_command));
    if (pcmd) {
        pcmd->stdinfd = open_file(&(cmdfrag->stdinfile), O_RDONLY);
        pcmd->stdoutfd = open_file(&(cmdfrag->stdoutfile),
                                   cmdfrag->stdoutfile_openflag);
        if (cmdfrag->stderr_to_stdout_flag == 1) {
            pcmd->stderrfd = 1;
        } else {
            pcmd->stderrfd = open_file(&(cmdfrag->stderrfile),
                                       cmdfrag->stderrfile_openflag);
        }

        /* handle open error */
        if (pcmd->stdinfd == -1 ||
            pcmd->stdoutfd == -1 ||
            pcmd->stderrfd == -1) {
            /* no need to free arglist */
            free(pcmd);
            return NULL;
        }

        size_t i = 0;
        for (; i < ARGSMAXCOUNT + 1; i++) {
            if (cmdfrag->arguments[i].str != NULL &&
                cmdfrag->arguments[i].len != 0) {
                pcmd->arglist[i] = make_arg(&(cmdfrag->arguments[i]));
            } else {
                pcmd->arglist[i] = NULL;
                break;
            }
        }
    }

    return pcmd;
}

void free_memory(struct pipe_command** pipecmds, size_t len) {
    for (size_t i = 0; i < len; i++) {
        freearglist((const char **)pipecmds[i]->arglist);
        free(pipecmds[i]);
    }
}

int parse_execute(const char* cmd, size_t cmdlen) {
    struct command_frag fragarray[MAXPIPECOUNT + 2];
    bzero(fragarray, sizeof(struct command_frag) *
            (MAXPIPECOUNT + 2));
    if (parse_command_with_pipe(cmd, cmdlen, fragarray) < 0) {
        return -1;
    }

    /* transform command_frag into pipe_command */
    struct pipe_command* pipesarray[MAXPIPECOUNT + 2];
    size_t pipearrayslen = 0;
    
    for (size_t i = 0; i < MAXPIPECOUNT + 2; i++) {
        if (fragarray[i].stderr_to_stdout_flag != -1) {
            struct pipe_command* pipecmd = 
                mk_pipecommand(fragarray + i);
            if (pipecmd == NULL) { /* may be malloc failed or open failed */
                /* free allocated memory */
                free_memory(pipesarray, pipearrayslen);
                return -1;
            }
            pipesarray[pipearrayslen++] = pipecmd;
        } else {
            break;
        }
    }

    /* call execute_command */
    int err = execute_command(pipesarray, pipearrayslen);
    
    /* free memory */
    free_memory(pipesarray, pipearrayslen);

    return err;
}


/*
 * ; ; ls ;
 */

/* struct command_frag fragarray[MAXPIPECOUNT + 2] */
int parse_and_execute_cmdline(const char* cmdline) {
    size_t rank = 0;
    size_t scmdbeg = rank;
    while (cmdline[rank] != '\0') {
        if ((rank == 0 || (rank > 0 && cmdline[rank - 1] != '\\')) &&
                cmdline[rank] == SPLITSIGN) {
            size_t cmdlen = rank - scmdbeg;
            /*
             * filter following cases:
             * ;;
             * ;  ;
             */
            if (cmdlen > 0 &&
                skip_whitespaces(cmdline + scmdbeg, cmdlen) < cmdlen) {
                int err = parse_execute(cmdline + scmdbeg, cmdlen);
                if (err < 0) {
                    return err;
                }
            }

            scmdbeg = rank + 1;
        }
        
        ++rank;
    }

    size_t cmdlen = rank - scmdbeg;
    if (cmdlen > 0 && skip_whitespaces(cmdline + scmdbeg, cmdlen) < cmdlen) {
        int err = parse_execute(cmdline + scmdbeg, cmdlen);
        if (err < 0) {
            return err;
        }
    }

    return 0;
}
