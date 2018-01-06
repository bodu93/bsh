#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: chown <user-name> <file>\n");
    exit(1);
}

uid_t userIdFromName(const char* name) {
    if (name == NULL || *name == '\0') /* null or empty string */
        return -1;

    /* allow a numeric name string */
    char* endptr;
    uid_t u = strtol(name, &endptr, 10);
    if (*endptr == '\0')
        return u;

    struct passwd* pwd = getpwnam(name);
    return (pwd == NULL) ? -1 : pwd->pw_uid;
}

gid_t groupId(const char* fpath) {
    struct stat statbuf;
    if (stat(fpath, &statbuf) < 0) {
        fprintf(stderr, "chown: stat %s error.\n", fpath);
        exit(1);
    }

    return statbuf.st_gid;
}


/*
 * do it trivially
 */
int main(int argc, char* argv[]) {
    if (argc != 3) usage();

    const char* username = argv[1];
    const char* filepath = argv[2];
    
    uid_t uid = userIdFromName(username);
    if (uid == (uid_t)-1) {
        fprintf(stderr, "chown: user %s does not exist.\n", username);
        exit(1);
    }

    /* same with chgrp... */
    if (chown(filepath, uid, groupId(filepath)) < 0) {
        fprintf(stderr, "chown: %s.\n", strerror(errno));
        exit(1);
    }

    exit(0);
}
