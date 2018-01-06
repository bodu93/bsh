#include "util.h"

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

void print_string_view(const struct string_view* sv) {
    assert(sv != NULL);

    char buf[sv->len + 1];
    snprintf(buf, sv->len + 1, "%s", sv->str);
    printf("%s", buf);
}

/*
 * make a copy of a string fragment which may not be null-terminated
 */
char* make_arg(const struct string_view* sv) {
    /* not null or empty string */
    assert(sv && sv->str && sv->str[0] != '\0');

    char* cp = (char*)malloc(sv->len + 1);
    if (cp) {
        cp[0] = 0;
        strncpy(cp, sv->str, sv->len);
        cp[sv->len] = 0;
    }

    return cp;
}

/*
 * free memory from make_arg
 */
void freearglist(const char* arglist[]) {
    for (size_t i = 0; arglist[i] != 0; i++) {
        free((char*)arglist[i]);
    }
}

size_t skip_whitespaces(const char* str, size_t len) {
    size_t rank = 0;
    while (rank < len && isblank(str[rank])) ++rank;

    return rank;
}

