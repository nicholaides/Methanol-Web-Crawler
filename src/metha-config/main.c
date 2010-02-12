#include <stdlib.h>
#include <stdio.h>
#include "config.h"

const char *cflags = BUILD_CFLAGS;
const char *libs   = BUILD_LIBS;

struct opt_result {
    char *opt;
    char *res;
} opt_results[] = {
    {"libs", BUILD_LIBS},
    {"cflags", BUILD_CFLAGS},
    {0}
};

int
main(int argc, char **argv)
{
    char *opt;
    int   x;

    if (argc == 2 && *(opt = argv[1]) == '-' && *(opt+1) == '-') {
        opt += 2;
        for (x = 0; opt_results[x].opt; x++) {
            if (strcmp(opt_results[x].opt, opt) == 0) {
                printf("%s\n", opt_results[x].res);
                return 0;
            }
        }
    }

    fprintf(stderr, "usage: metha-config [--libs, --cflags]\n");
    return 0;
}
