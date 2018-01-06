#ifndef BDU_SHELL_UTIL_H
#define BDU_SHELL_UTIL_H

#include <stdlib.h>

typedef struct string_view string_view;
struct string_view {
    /*
     * invariant: str || strlen(str) == len
     */
    const char*     str;
    size_t          len;
};

void print_string_view(const struct string_view* sv);
size_t skip_whitespaces(const char* str, size_t len);

char* make_arg(const struct string_view* sv);
void freearglist(const char* arglist[]);

#endif /* BDU_SHELL_UTIL_H */
