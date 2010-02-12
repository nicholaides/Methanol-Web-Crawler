/* *
 * URL Matching Expressions
 *
 * Author: Emil Romanus
 * License: None, put in Public Domain
 * */

/* * Definitions:
 * UMEX
 *   URL Matching Expressions
 * Part Identifier
 *   A string describing what part of the URL to match, 
 *   available part identifiers:
 *   FILE, FULL, HOST, PATH, DYN
 * Coffee
 *   Good
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "url.h"
#include "umex.h"

typedef struct umex EXPR;

#define U_SETLEN()  \
        if (*s)     \
            len = (*s)*256; \
        else                \
            len = 0;     \
        s++;             \
        len += (*s);     \
        s++

#define UMEX_SEARCH       0x00
# define UMEX_SEARCH_FULL  0x00
# define UMEX_SEARCH_PATH  0x01
# define UMEX_SEARCH_HOST  0x02
# define UMEX_SEARCH_VAR   0x03
# define UMEX_SEARCH_FILE  0x04

#define UMEX_STRMATCH8     0x01
#define UMEX_STRMATCH16    0x02
#define UMEX_STRFIND8      0x03
#define UMEX_STRFIND16     0x04
#define UMEX_STREND8       0x05
#define UMEX_STREND16      0x06
#define UMEX_STRBEGIN8     0x07
#define UMEX_STRBEGIN16    0x08
#define UMEX_ALWAYSMATCH   0x09
#define UMEX_BRACKET       0x0A
#define UMEX_ANYCHAR       0x0B
#define UMEX_NOT           0x0C

struct umex_search_descr {
    char *str; 
    int len;
    int val;
};

#define NUM_SDESC \
    sizeof(searchd)/sizeof(struct umex_search_descr)
struct umex_search_descr searchd[] = {
    { "PATH", 4, UMEX_SEARCH_PATH },
    { "HOST", 4, UMEX_SEARCH_HOST },
    { "FILE", 4, UMEX_SEARCH_FILE },
    { "FULL", 4, UMEX_SEARCH_FULL },
};

static unsigned int u_compile_str(EXPR* expr, char *start, char *end);
static unsigned int u_compile_bracket(EXPR* expr, char *s, char *e);
static inline void u_need_len(EXPR* expr, int len);

struct umex*
umex_dup(struct umex *u)
{
    struct umex *ret;

    if ((ret = malloc(sizeof(struct umex)))) {
        ret->sz = u->sz;
        ret->allocsz = u->allocsz;
        if (!(ret->bin = malloc(u->allocsz))) {
            free(ret);
            return 0;
        }
        memcpy(ret->bin, u->bin, ret->sz);
    }

    return ret;
}

int 
umex_match(url_t* url, EXPR* expr)
{
    char *str = 0;
    char *str_end = 0;
    char *s = expr->bin, 
         *end = s+expr->sz;
    unsigned short len;
    int not = 0;

    do {
        switch (*s) {
            case UMEX_SEARCH:
                s++;
                switch (*s) {
                    case UMEX_SEARCH_HOST:
                        str = url->str+url->host_o;
                        str_end = url->str+url->host_o+url->host_l;
                        break;
                    case UMEX_SEARCH_FILE:
                        str = url->str+url->file_o+1;
                        str_end = url->str+url->sz;
                        break;
                    case UMEX_SEARCH_FULL:
                        str = url->str;
                        str_end = url->str+url->sz;
                        break;
                    case UMEX_SEARCH_PATH:
                        str = url->str+url->host_l+url->host_o;
                        str_end = url->str+url->sz;
                        break;
                    default:
                    case UMEX_SEARCH_VAR:
                        printf("not implemented\n");
                        exit(0);
                        break;
                }
                s++;
                break;

            case UMEX_ANYCHAR:
                s++;
                str++;
                break;

            case UMEX_NOT:
                not = 1;
                break;

            /* match strings like "dsa" */
            case UMEX_STRMATCH16:
                s++;
                len = (*s)*256;
            case UMEX_STRMATCH8:
                s++;
                len = (*s);
                s++;
                if (str+len != str_end)
                    return 0;
                if (memcmp(str, s, len) != 0)
                    return 0;
                s+=len;
                str+=len;
                break;

            /* match strings like "*dsa" */
            case UMEX_STREND16:
                s++;
                len = (*s)*256;
            case UMEX_STREND8:
                s++;
                len = (*s);
                s++;
                if (len > str_end - str)
                    return 0;
                if (memcmp(str_end-len, s, len) != 0)
                    return 0;
                s+=len;
                return 1;

            /* match strings like "dsa*" */
            case UMEX_STRBEGIN16:
                s++;
                len = (*s)*256;
            case UMEX_STRBEGIN8:
                s++;
                len = (*s);
                s++;
                if (memcmp(str, s, len) != 0)
                    return 0;
                s+=len;
                str+=len;
                break;

            /* match strings like "*dsa*" */
            case UMEX_STRFIND16:
                s++;
                len = (*s)*256;
            case UMEX_STRFIND8:
                s++;
                len = (*s);
                s++;
                if (len > str_end - str)
                    return 0;
                char tmp1 = *str_end,
                     tmp2 = *(s+len);
                *str_end = '\0';
                *(s+len) = '\0';
                char *ret=strstr(str, s);
                *(s+len) = tmp2;
                *str_end = tmp1;
                if (!ret)
                    return 0;
                s+=len;
                str=ret+len;
                break;

            case UMEX_ALWAYSMATCH:
                return 1;
                break;

            default:
                printf("UMEX ERROR --%d--\n", __LINE__);
                umex_dump(expr);
                exit(0);
        }
    } while (s<end);

    if (s<end)
        return 0;
    return 1;
}

/** 
 * Generate a UMEX object with PATH<str*>
 *
 * used by the robots.txt parser in worker.c to quickly
 * generate disallow-strings.
 **/
EXPR*
umex_explicit_strstart(const char *str, unsigned int sz)
{
    umex_t *ret;

    if (!(ret = malloc(sizeof(umex_t))))
        return 0;

    if (!(ret->bin = malloc(sz+5))) {
        free(ret);
        return 0;
    }

    uint16_t size = (uint16_t)sz;

    ret->bin[0] = UMEX_SEARCH;
    ret->bin[1] = UMEX_SEARCH_PATH;
    ret->bin[2] = UMEX_STRBEGIN16;
    ret->bin[3] = (uint8_t)(size>>8);
    ret->bin[4] = (uint8_t)(size&0xff);
    ret->sz = size+5;
    ret->allocsz = size+5;

    memcpy(ret->bin+5, str, size);

    return ret;
}

/**
 * Translate human readable patterns to 
 * optimized binary patterns.
 *
 * Each new part identifier we receive as the human
 * readable pattern will be identified by first a 
 * UMEX_SEARCH byte directly followed by what type of 
 * part identifier it is when the pattern is 
 * compiled. Take the following pattern:
 *   PATH<Z>HOST<www.google.com>
 *
 * It will be translated into (ignore the line break):
 *   UMEX_SEARCH UMEX_SEARCH_PATH UMEX_STRMATCH 00 01 'Z'
 *   UMEX_SEARCH UMEX_SEARCH_HOST UMEX_STRMATCH 00 0E
 *   'w' 'w' 'w' '.' 'g' 'o' 'o' 'g' 'l' 'e' '.' 'c' 'o' 'm'
 *
 * The two bytes after the UMEX_STRMATCH indicates the length of 
 * the following string. The second length byte is the least 
 * significant.
 *
 * The UMEX_STRMATCH tells the umex_match() function that the 
 * following string must match identical to the path name of
 * the URL compared with.
 *
 **/
EXPR*
umex_compile(const char *str)
{
    char *s = (char*)str;
    char *ns;
    int mode = 0;
    int x;
    char c;
    char *tmp = 0;
    EXPR *ret = calloc(1, sizeof(EXPR));
    if (!ret)
        goto fail;
    ret->bin = malloc(256);
    ret->allocsz = 256;

    while (*s) {
        /* skip space and newline chars */
        if (*s <= 0x20) {
            s++;
            continue;
        }
        switch (mode) {
            case 1:
                c = *s;
                /* *
                 * c contains the character that will be used 
                 * as the part container separator. For example,
                 * if this character is '"' (quotation mark), then
                 * the expression probably looks something like this:
                 * PATH"content"
                 * 
                 * There are some special cases, when bracket chars are 
                 * used the ending character will be its corresponding 
                 * bracket. I.e. '>' for '<'.
                 * */
                switch (c) {
                    case '(':
                        c++;
                        break;
                    case '<':
                    case '{':
                    case '[':
                        c += 2;
                        break;
                }
                /* find the ending separator */
                s++;
                for (ns = s;; ns++) {
                    if (!*ns) {
                        ns = 0;
                        break;
                    }
                    /* verify that it's not escaped */
                    if (*ns == c && *(ns-1) != '\\')
                        break;
                }
                /* begin compiling expressions */
                if (ns) {
                    if (!u_compile_str(ret, s, ns))
                        goto fail;
                    s=ns+1;
                } else {
                    printf("UMEX ERROR:\n  Unterminated '%c'\n  Expression: \"%s\"\n", c, str);
                    goto fail;
                }
                mode = 0;
                break;

            default:
                for (x=0; ; x++) {
                    if (x==NUM_SDESC) {
                        /* if we didn't receive a part identifier,
                         * then default to UMEX_SEARCH_FILE */
                        int len = strlen(s);
                        tmp = malloc((3+len)*sizeof(char));
                        tmp[0] = '<';
                        memcpy(tmp+1, s, len);
                        tmp[len+1] = '>';
                        tmp[len+2] = '\0';
                        s = tmp;
                        ret->bin[ret->sz] = UMEX_SEARCH;
                        ret->bin[ret->sz+1] = UMEX_SEARCH_FILE;
                        ret->sz+=2;
                        mode = 1;
                        break;
                    }
                    if (strncmp(s, searchd[x].str, searchd[x].len) == 0) {
                        s+=searchd[x].len;
                        ret->bin[ret->sz] = UMEX_SEARCH;
                        ret->bin[ret->sz+1] = searchd[x].val;
                        ret->sz+=2;
                        mode = 1;
                        break;
                    }
                }
                continue;
        }
    }

    
    if (tmp)
        free(tmp);
    /*umex_dump(ret);*/
#ifdef DEBUG
    fprintf(stderr, "* umex:(%p) compiled expression '%s'\n", ret, str);
#endif
    return ret;

    fail:
    printf("UMEX ERROR ---%d---\n", __LINE__);
    umex_dump(ret);
    if (ret)
        free(ret);
    
    if (tmp)
        free(tmp);
    return 0;
}

/* make sure the needed length is allocated, 
 * and if not then run realloc() */
static inline void
u_need_len(EXPR* expr, int len)
{
    if (len>expr->allocsz) {
        do {expr->allocsz*=2;} while (len>expr->allocsz);
        expr->bin = realloc(expr->bin, expr->allocsz);
    }
}

/* *
 * Compile a bracket-contained expression
 *
 * Return true on success
 * */
static unsigned int 
u_compile_bracket(EXPR* expr, char *s /*start*/, char *e /*end*/)
{
    for (;s<e;s++) {
        if (*s<=0x20)
            continue;
        switch (*s) {
            case '!':
                u_need_len(expr, expr->sz+1);
                expr->bin[expr->sz] = UMEX_NOT;
                expr->sz++;
                break;
            case '?':
                u_need_len(expr, expr->sz+1);
                expr->bin[expr->sz] = UMEX_ANYCHAR;
                expr->sz++;
                break;
                /*
            case '\'';
            case '"':
            */
                
        }
    }
    return 1;
}

/* Returns trueeron success */
static unsigned int 
u_compile_str(EXPR* expr, char *start, char *end)
{
    unsigned int len = end - start,
                 cpylen;
    char *binp = expr->bin+expr->sz;
    /* one single '*' results in always match */
    if (len == 1 && *start == '*') {
        u_need_len(expr, expr->sz+1);
        expr->bin[expr->sz] = UMEX_ALWAYSMATCH;
        expr->sz++;
    } else {
        char *cs = start, *bs = start; /* current pointer */
        int wild_prev = 0;
        /* wild_prev is true or false whether the previous 
         * escape sequence was a wildcard */
        for (;;cs++) {
            /* if we're at the end of the expr */
            if (cs==end) {
                cpylen = cs-bs;
                if (cpylen) {
                    u_need_len(expr, expr->sz+3+cpylen);
                    binp = expr->bin+expr->sz;
                    if (cpylen<=255) {
                        binp[0] = (wild_prev == 0 ? UMEX_STRMATCH8 : UMEX_STREND8);
                        binp[1] = (char)cpylen;
                        binp+=2;
                        expr->sz+=2;
                    } else {
                        binp[0] = (wild_prev == 0 ? UMEX_STRMATCH16 : UMEX_STREND16);
                        binp[1] = (char)(cpylen/256);
                        binp[2] = (char)(cpylen%256);
                        binp+=3;
                        expr->sz+=3;
                    }
                    memcpy(binp, bs, cpylen);
                    expr->sz+=cpylen;
                }
                break;
            }
            if ((*cs == '*' && *(cs-1) != '\\')
                || (*cs == '[' && *(cs-1) != '\\')) {
                cpylen = cs-bs;
                if (!cpylen) {
                    bs = cs+1;
                    wild_prev = 1;
                } else {
                    u_need_len(expr, expr->sz+3+cpylen);
                    binp = expr->bin+expr->sz;
                    if (cpylen <= 255) {
                        if (wild_prev == 1)
                            binp[0] = UMEX_STRFIND8;
                        else
                            binp[0] = UMEX_STRBEGIN8;
                        binp[1] = (char)cpylen;
                        binp+=2;
                        expr->sz+=2;
                    } else {
                        if (wild_prev == 1)
                            binp[0] = UMEX_STRFIND16;
                        else
                            binp[0] = UMEX_STRBEGIN16;
                        binp[1] = (char)(cpylen/256);
                        binp[2] = (char)(cpylen%256);
                        binp+=3;
                        expr->sz+=3;
                    }
                    memcpy(binp, bs, cpylen);
                    expr->sz+=cpylen;
                    binp+=cpylen;
                    wild_prev = 1;
                }
                if (*cs == '[') {
                    int b_count = 1; /* bracket counter */
                    char *tmp;
                    for (tmp=cs+1;; tmp++) {
                        if (tmp==end) {
                            printf("UMEX ERROR:\n  Unterminated '['\n");
                            return 0;
                        }
                        if (*tmp == ']') {
                            b_count--;
                            if (!b_count)
                                break;
                        } else if (*tmp == '[')
                            b_count++;
                    }
                    if (!u_compile_bracket(expr, cs+1, tmp))
                        return 0;
                    wild_prev = 0;
                    binp = expr->bin+expr->sz;
                    cs=tmp;
                }
                bs = cs+1;
            }
        }
    }
    return 1;
}

void 
umex_dump(EXPR* expr)
{
    printf("begin dump\n");
    printf("  (struct umex*) (%p)\n", expr);
    printf("  %d bytes\n", (int)sizeof(EXPR));
    printf("unsigned int\n");
    printf("  %d\n", expr->sz);
    printf("code\n");
    int x;
    printf("  ");
    for (x=0; x<expr->sz; x++) {
        if (expr->bin[x]>=0x20) {
            do {
                printf("%c", expr->bin[x]);
                x++;
            } while (x<expr->sz && expr->bin[x] >= 0x20);
            printf(",");
        }
        if (x<expr->sz)
            printf("%#x,", expr->bin[x]);
    }
    puts("");
    printf("end dump\n");
}

void 
umex_free(EXPR* expr)
{
    if (expr) {
        if (expr->bin)
            free(expr->bin);
        free(expr);
    }
}
