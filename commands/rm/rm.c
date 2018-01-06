#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: rm [-d][-r] <rm-path>\n");
    exit(1);
}

int dflag;      /* directory */
int rflag;      /* recursive */
char* rmpath;   /* rm file(or dir) path */
void parseCmdLine(int argc, char* argv[]) {
    int ch;
    while ((ch = getopt(argc, argv, "dr")) != -1) {
        switch (ch) {
            case 'd':
                dflag = 1;
                break;
            case 'r':
                rflag = 1;
                break;
        }
    }
    argv += optind;
    rmpath = *argv;
}

#ifdef PATH_MAX
static long pathmax = PATH_MAX;
#else
static long pathmax = 0;
#endif

static long posix_version = 0;
static long xsi_version = 0;

#define PATH_MAX_GUESS 1024

static char* path_alloc(size_t *sizep) {
    if (posix_version == 0)
        posix_version = sysconf(_SC_VERSION);

    if (xsi_version == 0)
        xsi_version = sysconf(_SC_XOPEN_VERSION);

    if (pathmax == 0) {
        errno = 0;
        if ((pathmax = pathconf("/", _PC_PATH_MAX)) < 0) {
            if (errno == 0)
                pathmax = PATH_MAX_GUESS; /* it is indeterminate */
            else {
                fprintf(stderr, "rm: pathconf error for _PC_PATH_MAX\n");
                exit(1);
            }
        } else {
            pathmax++;
        }
    }

    size_t size;
    if ((posix_version < 200112L) && (xsi_version < 4))
        size = pathmax + 1;
    else
        size = pathmax;

    char* ptr;
    if ((ptr = (char*)malloc(size)) == NULL) {
        fprintf(stderr, "rm: malloc error for pathname\n");
        exit(1);
    }
    
    if (sizep != NULL)
        *sizep = size;
    return ptr;
}

/*
 * 1 to dir
 * 0 to file or other filetypes
 */
int filetype(const char* fpath) {
    struct stat statbuf;
    if (stat(fpath, &statbuf) < 0) {
        fprintf(stderr, "rm: stat error for %s\n", fpath);
        exit(1);
    }

    return S_ISDIR(statbuf.st_mode);
}


void recursiverm(const char* path) {
    DIR *dir = opendir(path);
    if (dir == NULL) return;
    
    struct dirent* dent;
    while ((dent = readdir(dir)) != NULL) {
        /* filter out . and .. */
        if (strcmp(dent->d_name, ".") != 0 &&
            strcmp(dent->d_name, "..") != 0) {
            size_t pathlen = 0;
            char* abspath = path_alloc(&pathlen);
            if (pathlen > strlen(path) + dent->d_namlen + 1) {
                strcpy(abspath, path);
                if (path[strlen(path) - 1] != '/') strcat(abspath, "/");
                strcat(abspath, dent->d_name);
                if (filetype(abspath) == 0) {
                    if (remove(abspath) < 0) {
                        fprintf(stderr, "rm: remove error for %s\n", abspath);
                        exit(1);
                    }
                } else {
                    recursiverm(abspath);
                }
            } else {
                fprintf(stderr, "rm: files in %s has been partial removed for unsufficient path length\n", abspath);
                exit(1);
            }
            free(abspath);
        }
    }
    closedir(dir);
    
    /* and next remove empty dir itself */
    if (remove(path) < 0) {
        fprintf(stderr, "rm: remove %s error %s\n", rmpath, strerror(errno));
        exit(1);
    }
}


void dorm() {
    if (!dflag && !rflag) { /* rm <path> */
       if (remove(rmpath) < 0) {
           fprintf(stderr, "rm: remove %s error %s\n", rmpath, strerror(errno));
           exit(1);
       }
    } else if (dflag && !rflag) { /* rm -d <path> */
        if (rmdir(rmpath) < 0) {
            fprintf(stderr, "rm: rmdir %s error %s\n", rmpath, strerror(errno));
            exit(1);
        }
    } else if (!dflag && rflag) { /* rm -f <path> */
        if (filetype(rmpath) == 1) { /* rmpath is a dir */
            fprintf(stderr, "rm: %s is a directory\n", rmpath);
            exit(1);
        }

        if (unlink(rmpath) < 0) {
            fprintf(stderr, "rm: unlink %s error %s\n", rmpath, strerror(errno));
            exit(1);
        }
        
    } else { /* rm -df <path> */
        if (filetype(rmpath) != 1) {
            fprintf(stderr, "rm: %s is not a directory\n", rmpath);
            exit(1);
        } else {
            /* remove all files in directory recursively */
            recursiverm(rmpath);
        }
    }
}


int main(int argc, char* argv[]) { 
    parseCmdLine(argc, argv);
    if (rmpath == NULL) usage();

    /* It is an error to attempt to remove the files "." or ".." */
    if (strcmp(rmpath, ".") == 0 || strcmp(rmpath, "..") == 0) {
        fprintf(stderr, "rm: %s can't remove\n", rmpath);
        exit(1);
    } else {
        dorm();
    }

    exit(0);
}    
