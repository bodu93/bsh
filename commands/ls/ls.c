#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: %s [-l][-a] [<dir-path>]\n", "ls");
    exit(1);
}

char* userNameFromId(uid_t uid) {
    struct passwd* pwd;

    pwd = getpwuid(uid);
    return (pwd == NULL) ? NULL : pwd->pw_name;
}

char* groupNameFromId(gid_t gid) {
    struct group* grp;

    grp = getgrgid(gid);
    return (grp == NULL) ? NULL : grp->gr_name;
}

int lflag;
int aflag;
char* lspath;
void parseCmdLine(int argc, char* argv[]) {
    int ch = 0;
    while ((ch = getopt(argc, argv, "la")) != -1) {
        switch (ch) {
            case 'l':
                lflag = 1;
                break;
            case 'a':
                aflag = 1;
                break;
            default:
                usage();
                break;
        }
    }
    argv += optind;
    argc -= optind;
    if (argc == 0)
        lspath = ".";
    else
        lspath = *argv;
}

char filetype(const struct stat* fstate) {
    mode_t fmode = fstate->st_mode;

    char ft = '-';
    if (S_ISREG(fmode)) ft = '-';
    else if (S_ISDIR(fmode)) ft = 'd';
    else if (S_ISCHR(fmode)) ft = 'c';
    else if (S_ISBLK(fmode)) ft = 'b';
    else if (S_ISFIFO(fmode)) ft = 'f';
    else if (S_ISLNK(fmode)) ft = 'l';
    else if (S_ISSOCK(fmode)) ft = 's';
    else ft = '?';

    return ft;
}

void modestr(const struct stat* fstate, char* modebuf, size_t buflen) {
    mode_t fmode = fstate->st_mode;

    mode_t allmodes[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
    };
    for (size_t i = 0; i != buflen; i++) {
        size_t rank = i % 3;
        int r = fmode & allmodes[i];
        switch (rank) {
            case 0:
                modebuf[i] = r ? 'r' : '-';
                break;

            case 1:
                modebuf[i] = r ? 'w' : '-';
                break;

            case 2:
                modebuf[i] = r ? 'x': '-';
                modebuf[i] = S_ISUID & fmode ? 's' : modebuf[i];
                break;
        }
    }
}

void modifytime(const struct stat* fstate, char* timebuf, size_t buflen) {
    time_t tt = fstate->st_mtimespec.tv_sec;
    strftime(timebuf,
            buflen - 1,
            "%H:%M",
            localtime(&tt));
}

void ppdir(const char* absdpath, char* dirpath, size_t pathlen) { /* FixMe: buffer overflow */
    const char* endptr = strrchr(absdpath, '/');
    if (endptr == absdpath) { /* root dir */
        dirpath[0] = *absdpath;
        dirpath[1] = '\0';
    } else {
        strncpy(dirpath, absdpath, endptr - dirpath);
        dirpath[endptr - dirpath] = '\0';
    }
}

/* get absolute file path */
void absfilepath(const char* dirpath, const char* fname, char* fpath, size_t pathlen) {
    if (strcmp(fname, ".") == 0) {
        strncpy(fpath, dirpath, pathlen - 1);
    } else if (strcmp(fname, "..") == 0) { /* parent directory of dirpath */
        char cwd[BUFSIZ];
        getcwd(cwd, BUFSIZ); 
        char pcwd[BUFSIZ];
        ppdir(cwd, pcwd, BUFSIZ);
    } else {
        strncpy(fpath, dirpath, pathlen - 1);
        if (dirpath[strlen(dirpath) - 1] != '/') strncat(fpath, "/", 1);
        strncat(fpath, fname, pathlen - 1 - strlen(dirpath));
    }

    fpath[pathlen - 1] = '\0';
}

void showinfo(const char* dirpath, const char* name) {
    /* null or empty name string */
    if (name == NULL || *name == '\0') return;
    /* hide shadow file if -a option not specified */
    if (aflag == 0 && *name == '.') return;

    if (lflag == 0) {
        printf("%s\n", name);
    } else {
        /* get absolute file path */
        char fpath[BUFSIZ];
        absfilepath(dirpath, name, fpath, BUFSIZ);

        struct stat fstate;
        if (stat(fpath, &fstate) == 0) {
            /* file type */
            char ftype = filetype(&fstate);

            /* file mode string */
            char mstr[10];
            modestr(&fstate, mstr, 9);
            mstr[9] = '\0';

            /* hard links number */
            nlink_t ln = fstate.st_nlink;

            /* uername */
            char* username = userNameFromId(fstate.st_uid);

            /* group name */
            char* groupname = groupNameFromId(fstate.st_gid);

            /* blocks number */
            blkcnt_t nblock = fstate.st_blocks;

            /* last modify time */
            char lastmodify[BUFSIZ];
            modifytime(&fstate, lastmodify, BUFSIZ);
            lastmodify[BUFSIZ - 1] = '\0';

            /* simple output */
            printf("%c%9s  %-4lu %-10s %-10s %-6lu %-6s %s\n",
                    ftype,
                    mstr,
                    (unsigned long)ln,
                    username,
                    groupname,
                    (unsigned long)nblock,
                    lastmodify,
                    name);
        } else {
            /* ignore stat error?? */
            fprintf(stderr, "stat error for %s\n", name);
        }
    }
}

void listdir(const char* dirpath) {
    DIR* dir = opendir(dirpath);
    if (dir == NULL) {
        fprintf(stderr, "opendir error: %s\n", dirpath);
        exit(1);
    }

    struct dirent* dent;
    while ((dent = readdir(dir)) != NULL) {
        showinfo(dirpath, dent->d_name);
    }
    
    closedir(dir);
}


int main(int argc, char* argv[]) {
    parseCmdLine(argc, argv);
    listdir(lspath);

    return 0;
}
