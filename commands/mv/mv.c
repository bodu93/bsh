#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define max(x, y) ((x) > (y)) ? (x) : (y)
#define min(x, y) ((x) < (y)) ? (x) : (y)


void usage() {
    fprintf(stderr, "usage: mv src dst\n");
    exit(1);
}

void error(const char* srcpath, const char* dstpath) {
    /* FixMe: modify error string */
    fprintf(stderr, "mv: rename %s to %s error: %s\n",
            srcpath, dstpath, strerror(errno)
           );
    exit(1);
}

/*
 * check file type of path
 * -1 for error
 *  0 for file
 *  1 for directory
 */
int filetype(const char* path) {
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) return -1;
    
    return S_ISDIR(statbuf.st_mode) ? 1 : 0;
} 

int pathexist(const char* path) {
    return (access(path, F_OK) == 0) ? 0 : -1;
}

const char* basename(const char* fpath) {
    const char* endptr = strrchr(fpath, '/');
    return (endptr == NULL) ? fpath : endptr + 1;
}

void ftof(const char* srcpath, const char* dstpath) {
    if (rename(srcpath, dstpath) < 0) {
        error(srcpath, dstpath);
    }
}

void ftod(const char* srcpath, const char* dstpath) {
    const char* fname = basename(srcpath);

    char* dst = (char*)malloc(strlen(dstpath) + strlen(fname) + 2);
    strcat(dst, dstpath);
    if (dstpath[strlen(dstpath) - 1] != '/') strcat(dst, "/");
    strcat(dst, fname);
    assert(strncmp(dst, dstpath, min(strlen(dst), strlen(dstpath))) == 0 &&
                "concate dst path error!");
    ftof(srcpath, dst);
    free(dst);
}

void dtof(const char* srcpath, const char* dstpath) {
    /* dstpath exist: unlink it */
    if (pathexist(dstpath) == 0 && unlink(dstpath) < 0) {
        error(srcpath, dstpath);
    }

    ftof(srcpath, dstpath);
}

/*
 * rename directory to directory
 * dstpath must be a empty directory
 */
void dtod(const char* srcpath, const char* dstpath) {
    /* allocate dst path */
    char* src = (char*)malloc(strlen(srcpath) + 1);
    strcat(src, srcpath);
    size_t srclen = strlen(srcpath);
    if (srclen != 1 && srcpath[srclen - 1] == '/')
        src[srclen - 1] = '\0';
    assert(strncmp(src, srcpath, min(strlen(src), strlen(srcpath))) == 0 &&
                "assign dst path error!");

    const char* sdname = basename(src);
    char* dst = (char*)malloc(strlen(dstpath) + strlen(sdname) + 2);
    strcat(dst, dstpath);
    strcat(dst, "/");
    strcat(dst, sdname);

    assert(strncmp(dst, dstpath, min(strlen(dst), strlen(dstpath))) == 0 &&
                "concate dst path error!");

    if (rename(srcpath, dst) < 0)
        error(srcpath, dstpath);

    free(dst);
    free(src);
}
    
void domove(const char* srcpath, const char* dstpath) {
    /* cannot mv root dir?? */
    if (strlen(srcpath) == 1 && srcpath[0] == '/')
        error(srcpath, dstpath);

    /* srcpath does not exist */
    if ((pathexist(srcpath) < 0)) error(srcpath, dstpath);
   
    /* dstpath does not exist */
    if (pathexist(dstpath) < 0) {
        /* just rename */
        if (rename(srcpath, dstpath) < 0)
            error(srcpath, dstpath);
    } else {
        int srctype = filetype(srcpath);
        int dsttype = filetype(dstpath);
        if (srctype == 0 && dsttype == 0)
            ftof(srcpath, dstpath);
        else if (srctype == 0 && dsttype == 1)
            ftod(srcpath, dstpath);
        else if (srctype == 1 && dsttype == 0)
            dtof(srcpath, dstpath);
        else if (srctype == 1 && dsttype == 1)
            dtod(srcpath, dstpath);
        else /* cannot mv other file type?? */
            error(srcpath, dstpath);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) usage();

    domove(argv[1], argv[2]);

    exit(0);
}
