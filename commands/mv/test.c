#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    /*
     * link(oldname, newname)
     * if oldname is a directory and newname does exist
     * errno == 1: Operation permitted
     * why?????
     */
    if (link(argv[1], argv[2]) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    } else {
        unlink(argv[1]);
    }

    exit(0);
}
