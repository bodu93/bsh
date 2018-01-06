/*
 * touch utility -- change file access and modification time.
 * -a: change access time
 * -m: change modification time
 * -t: specifies time value
 */
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: touch [-a] [-m] [-t [[CC]YY]MMDDhhmm[.SS]] <file>\n");
    exit(1);
}

int aflag;
int mflag;
int tflag;
char* touchtime;
char* touchfile;
void parsecmdline(int argc, char* argv[]) { /* FIXME: STRONG-CHECK */
    int ch;
    while ((ch = getopt(argc, argv, "amt:")) != -1) {
        switch (ch) {
            case 'a':
                aflag = 1;
                break;
            case 'm':
                mflag = 1;
                break;
            case 't':
                touchtime = optarg;
                tflag = 1;
                break;
        }
    }
    argv += optind;
    touchfile = *argv;
}

void formattimestr() {
    char timestr[16];
    timestr[0] = '\0';

    char* dotptr = strchr(touchtime, '.');
    int spseconds = 0;
    if (dotptr != NULL) { /* FIXME: wrong time format */
        spseconds = 1;
    }

    size_t touchtimelen = strlen(touchtime) - (spseconds ? 3 : 0);
    if (touchtimelen == 10) { /* no CC */
        char* endptr = touchtime + 2;
        long year = strtol(touchtime, &endptr, 10);
        if (year >= 69 && year <= 99) {
            strcat(timestr, "19");
        } else {
            strcat(timestr, "20");
        }
    } else if (touchtimelen == 8) { /* no CC or YY */
        char yearstr[5];
        yearstr[0] = '\0';
        time_t timet = time(NULL);
        strftime(yearstr, 5, "%Y", localtime(&timet));
        strcat(timestr, yearstr);
    }
    strcat(timestr, touchtime);
    if (spseconds == 0) strcat(timestr, ".00");

    touchtime = timestr;
}

void dotouch(const struct timespec* ts) {
    struct timespec tspec[2];
    tspec[0].tv_nsec = UTIME_OMIT;
    tspec[1].tv_nsec = UTIME_OMIT;
    if (aflag) tspec[0] = *ts;
    if (mflag) tspec[1] = *ts;
    
    int fd = open(touchfile, O_WRONLY); /* open to write?? */
    if (fd < 0) {
        fprintf(stderr, "touch: open %s error\n", touchfile);
        exit(1);
    }
    if (futimens(fd, tspec) < 0) {
        fprintf(stderr, "touch: futimens error %s\n", strerror(errno));
        exit(1);
    }
    close(fd);
}

int main(int argc, char* argv[]) {
    parsecmdline(argc, argv);
    if (touchfile == NULL) usage();

    if (!aflag && !mflag) { /* specifies -a and -m options by default */
        aflag = 1;
        mflag = 1;
    }

    struct timespec ts;
    if (!tflag) { /* doesn't specifies -t option: get current time */
        ts.tv_sec = 0;
        ts.tv_nsec = UTIME_NOW;
    } else {
        formattimestr();

        struct tm touchtm;
        char *endptr = strptime(touchtime, "%Y%m%d%H%M.%S", &touchtm);
        if (endptr == NULL) {
            fprintf(stderr, "touch: strptime error %s\n", strerror(errno));
            exit(1);
        }
        time_t tt = mktime(&touchtm);
        if (tt == -1) {
            fprintf(stderr, "touch: mktime error %s\n", strerror(errno));
            exit(1);
        }
        ts.tv_sec = tt;
        ts.tv_nsec = 0;
    }

    dotouch(&ts); 

    exit(0);
}
