/* *
 * URL Matching Expressions
 *
 * Author: Emil Romanus
 * License: None, put in Public Domain
 * */

#ifndef _UMEX__H_
#define _UMEX__H_

typedef struct umex {
    unsigned int sz;
    unsigned int allocsz;
    char *bin;
} umex_t;

int umex_match(struct url *url, struct umex *expr);
struct umex* umex_compile(const char *str);
struct umex* umex_explicit_strstart(const char *str, unsigned int sz);
void umex_free(struct umex *expr);
void umex_dump(struct umex *expr);
struct umex* umex_dup(struct umex* u);

#endif
