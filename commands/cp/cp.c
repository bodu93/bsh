#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define RDBUFLEN 4096

void usage() {
    fprintf(stderr, "usage: cp <src-file> <dst-file>|<dst-dir>\n");
    exit(1);
}

/*
 * can copy "file hole"
 */
void ftof(const char* srcpath, const char* dstpath) {
    struct stat statbuf;
    if (stat(srcpath, &statbuf) < 0) {
        fprintf(stderr, "cp: stat error for %s\n", srcpath);
        exit(1);
    }

    /* for simplicity: src-file cannot be a regulare file */
    if (S_ISREG(statbuf.st_mode) == 0) {
        fprintf(stderr, "cp: %s is not a regular file\n", srcpath);
        exit(1);
    }

    int srcfd;
    if ((srcfd = open(srcpath, O_RDONLY)) < 0) {
        fprintf(stderr, "cp: open error for %s\n", srcpath);
        exit(1);
    }

    int dstfd;
    if ((dstfd = open(dstpath, O_WRONLY | O_TRUNC | O_CREAT, 0777)) < 0) {
        fprintf(stderr, "cp: open error for %s\n", dstpath);
        exit(1);
    }

    char buf[RDBUFLEN + 1];
    ssize_t rdcnt = 0;
    while ((rdcnt = read(srcfd, buf, RDBUFLEN)) > 0) {
        buf[RDBUFLEN] = '\0';

        size_t i = 0;
        size_t wbeg = 0;    /* write begin */
        size_t lbeg = 0;    /* lseek begin */
        while (i != (size_t)rdcnt) {
            if (buf[i] != 0) {
                wbeg = i;
                /* loop util encounter a 'file hole' */
                while (i != (size_t)rdcnt && buf[i] != '\0') ++i;
                ssize_t writelen = i - wbeg;
                if (write(dstfd, buf + wbeg, writelen) != writelen) {
                    fprintf(stderr, "cp: write error for %s\n", dstpath);
                    exit(1);
                }
            }

            lbeg = i;
            /* loop util encounter a 'character' */
            while (i != (size_t)rdcnt && buf[i] == '\0') ++i;
            ssize_t lseeklen = i - lbeg;
            if (lseek(dstfd, lseeklen, SEEK_CUR) == -1) {
                fprintf(stderr, "cp: lseek error for %s\n", strerror(errno));
                exit(0);
            }
        }
    }

    if (-1 == rdcnt) {
        fprintf(stderr, "cp: read error for %s\n", srcpath);
        exit(1);
    }

    close(srcfd);
    close(dstfd);
}

void ftodir(const char* fpath, const char* dirpath) {
    const char* basename = strchr(fpath, '/');
    if (basename == NULL) basename = fpath;
    else basename += 1;

    char* dstpath = (char*)malloc(strlen(dirpath) + strlen(basename) + 2);
    dstpath[0] = 0;
    strcpy(dstpath, dirpath);
    if (dstpath[strlen(dstpath) - 1] != '/') strcat(dstpath, "/");
    strcat(dstpath, basename);
    /* just link */
    if (link(fpath, dstpath)) {
        fprintf(stderr, "cp: link error for %s\n", strerror(errno));
        free(dstpath);
        exit(1);
    }
    free(dstpath);
}

void handle_all_input_error(const char* srcpath, const char* dstpath) {
    if (strcmp(srcpath, dstpath) == 0) {
        fprintf(stderr, "cp: %s and %s are identical\n", srcpath, dstpath);
        exit(1);
    }

    int exist = access(srcpath, F_OK);
    if (exist < 0) {
        fprintf(stderr, "cp: %s does not exists!\n", srcpath);
        exit(1);
    }
}

void docopy(const char* srcpath, const char* dstpath) {
    handle_all_input_error(srcpath, dstpath);

    struct stat statbuf;
    int err = stat(dstpath, &statbuf);
    if (err == 0 && S_ISDIR(statbuf.st_mode)) {
        ftodir(srcpath, dstpath);
    } else {
        ftof(srcpath, dstpath);
    }
}

int main(int argc, char* argv[]) {
   if (argc != 3) usage(); 

   const char* srcpath = argv[1];
   const char* dstpath = argv[2];

   docopy(srcpath, dstpath);

   exit(0);
}
