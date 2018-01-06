#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

void usage() {
    fprintf(stderr, "usage: chmod <permission> <file>\n");
    exit(1);
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

/*
 * do it trivially
 */
int main(int argc, char* argv[]) {
    if (argc != 3) usage();

    const char* permission = argv[1];
    const char* filepath = argv[2];

    mode_t usermode = strtomode(permission);
    if (chmod(filepath, usermode) < 0) {
        fprintf(stderr, "chmod: %s\n", strerror(errno));
        exit(1);
    }

    exit(0);
}
