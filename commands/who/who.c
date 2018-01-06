#include <utmpx.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: %s\n", "who");
    exit(1);
}

void show_info(struct utmpx* info) {
    printf("%-8.8s", info->ut_user);
    printf(" ");
    printf("%-8.8s", info->ut_line);
    printf(" ");

    time_t tvsec = info->ut_tv.tv_sec;
    char timebuf[BUFSIZ];
    size_t timelen = strftime(timebuf, BUFSIZ - 1, "%b %d %H:%M", localtime(&tvsec));
    printf("%s", timelen == 0 ? "unknown login time" : timebuf);
    printf("\n");
}


int main(int argc, char* argv[]) {
    if (argc != 1) usage();

    setutxent();
    struct utmpx* info;
    while ((info = getutxent()) != NULL) {
        if (info->ut_type == LOGIN_PROCESS || info->ut_type == USER_PROCESS)
            show_info(info);
    }
    endutxent();

    exit(0);
}
