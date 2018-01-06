#include <unistd.h>
#include <fcntl.h>
#include <utmpx.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage() {
    fprintf(stderr, "usage: write user [tty]\n");
    exit(1);
}

void do_write(const char* ttytowrite) {
    size_t pathlen = _UTX_LINESIZE + 5;
    char* ttypath = (char*)malloc(pathlen + 1);
    ttypath[0] = '\0';
    strcat(ttypath, "/dev/");
    strcat(ttypath, ttytowrite);
    
    int fd = open(ttypath, O_WRONLY);
    if (fd < 0) {
        perror("open");
        free(ttypath);
        exit(1);
    }

    /* get local machine hostname */
    char hostname[1024];
    hostname[1023] = '\0';
    hostname[0] = '@';
    int err = gethostname(hostname + 1, 1023 - 1);

    /* get current local time */
    char timebuf[BUFSIZ];
    time_t now = time(NULL);
    size_t timelen = strftime(timebuf, BUFSIZ, "%H:%M", localtime(&now));

    /* write prompt to another tty */
    char prompt[BUFSIZ];
    snprintf(prompt, BUFSIZ, "\nMessage from %s%s on %s at %s\n",
            getlogin(),
            err < 0 ? "" : hostname,
            ttyname(STDIN_FILENO),
            timelen == 0 ? "unknown time" : timebuf);
    write(fd, prompt, strlen(prompt));

    /* get message from stdin and write it all to tty */
    char buf[BUFSIZ];
    while (fgets(buf, BUFSIZ, stdin) != NULL)
        if (write(fd, buf, strlen(buf)) != strlen(buf))
            break;
    close(fd);
    free(ttypath);
}

int findLoginInfo(const char* username, const char* tty, char **result) {
    int find = 0;
    char* resulttty = NULL;

    setutxent();

    struct utmpx* info = NULL;
    while ((info = getutxent()) != NULL) {
        if (strncmp(username, info->ut_user, _UTX_USERSIZE) == 0 &&
            (info->ut_type == LOGIN_PROCESS || info->ut_type == USER_PROCESS)) {
            if (!tty && strncmp("tty", info->ut_line, 3) == 0) {
                resulttty = (char*)malloc(_UTX_LINESIZE);
                find = 1;
                strncpy(resulttty, info->ut_line, _UTX_LINESIZE);
                break;
            }

            if (tty && strncmp(tty, info->ut_line, _UTX_LINESIZE)) {
                find = 1;
                resulttty = (char*)malloc(_UTX_LINESIZE);
                strncpy(resulttty, info->ut_line, _UTX_LINESIZE);
                break;
            }
        }
    }

    endutxent();
    *result = resulttty;

    return find;
}

    
int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) usage();

    char* username = argv[1];
    char* ttyname = argc > 3 ? argv[2] : NULL;

    char* findtty = NULL;
    int find = findLoginInfo(username, ttyname, &findtty);
    
    if (find) {
        if (findtty) {
            do_write(findtty);
            free(findtty);
        }
        else {
            fprintf(stderr, "write: user %s does login with tty %s\n",
                    username, ttyname);
            exit(1);
        }
    } else {
        fprintf(stderr, "write: user %s does not login or does not login with tty\n", username);
        exit(1);
    }

    exit(0);
}
