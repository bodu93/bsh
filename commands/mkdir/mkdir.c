#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: mkdir [-m <perm-bit>] [-p] <dirpath>\n");
    exit(1);
}

int pflag;      /* create dirpath recursively */
int mflag;      /* create dirpath with specified */
char* mode;
char* dirpath;
void parseCmdLine(int argc, char* argv[]) { /* FixMe: STRONG-CHECK */
    int ch;
    while ((ch = getopt(argc, argv, "pm:")) != -1) {
        switch (ch) {
            case 'p':
                pflag = 1;
                break;
            case 'm':
                mflag = 1;
                mode = optarg;
                break;
        }
    }
    argv += optind;
    dirpath = *argv;
}

mode_t curumask() {
    mode_t curmask = umask(0);
    umask(curmask);

    return curmask;
}

mode_t strtomode(const char* modestr) {
    mode_t allmodes[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
    };
    const char* allmodestr = "rwxrwxrwx";

    mode_t resultmode = 0;
    size_t modelen = strlen(modestr);
    
    const char* endptr = allmodestr;
    size_t i = 0;
    while (i < modelen) {
        if (*endptr == modestr[i]) {
            resultmode |= allmodes[endptr - allmodestr];
            ++i;        
        } else if (modestr[i] == '-') { /* let '-' hold place */
            ++i;
        }
        ++endptr;
    }
    return resultmode;
}

#define min(x, y) ((x) < (y)) ? (x) : (y)

int pathexist(const char* path) {
    return access(path, F_OK);
}

void Mkdir(const char* path, mode_t mode) {
    if (mkdir(path, mode) < 0) {
        fprintf(stderr, "mkdir: mkdir error for %s\n", strerror(errno));
        exit(1);
    }
}

void domkdir(int persist, const char* modestr, const char* cpath) {
    mode_t creationmode = strtomode(modestr);
    if (persist == 0) { /* don't create intermediate dir */
        Mkdir(cpath, creationmode);
    } else {
        /* printf("cpath %s\n", cpath); */
        char realpath[1024 + 2];  /* PATH_MAX?? */
        realpath[0] = '\0';       /* keep strcpy and strcat correct! */
        if (*cpath != '/' || *cpath != '.') strcpy(realpath, "./");
        strncat(realpath, cpath, min(strlen(cpath) + 1, sizeof(realpath) - strlen(realpath) - 1));
        realpath[sizeof(realpath) - 1] = '\0';

        /* printf("real path %s\n", realpath); */

        /* create intermediate path */
        const char* begin = realpath;
        const char* slash = strchr(realpath, '/');
        char interpath[1024];   /* PATH_MAX?? */
        interpath[0] = '\0';
        while (slash != NULL) { 
            strncat(interpath, begin, slash - begin);
            strcat(interpath, "\0");
            /* printf("inter path %s\n", interpath); */
            if (pathexist(interpath) < 0) { /* path not exist */
                Mkdir(interpath, creationmode);
            }

            begin = slash;
            slash = strchr(slash + 1, '/');
        }
        /* create the real path */
        Mkdir(cpath, creationmode);
    }
}

int main(int argc, char* argv[]) {
    parseCmdLine(argc, argv);
    if (dirpath == NULL) usage();

    char defmode[] = "rwxrwxrwx";       /* default mode string: 0777 */
    domkdir(pflag, mflag ? mode : defmode, dirpath);

    /* printf("p: %d, m: %d, mode = %s, dirpath = %s\n", pflag, mflag, mode, dirpath); */

    return 0;
}
