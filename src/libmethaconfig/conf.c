/*-
 * conf.c
 * This file is part of libmethaconfig
 *
 * Copyright (C) 2008-2009, Emil Romanus <emil.romanus@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * http://bithack.se/projects/methabot/
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "lmc.h"

/* set maximum configuration file size to 1 MB */
#define MAX_FILESIZE 1024*1024

static int  sgetline(const char *start, const char *pos);
static void set_error(lmc_parser_t *lmc, const char *e, ...);

/* parser states */
enum {
    STATE_ROOT,
    STATE_PRE_NAME,
    STATE_NAME,
    STATE_POST_NAME,
    STATE_END_NAME,
    STATE_PRE_OBJ,
    STATE_OBJ,
    STATE_PRE_VAL,
    STATE_VAL,
    STATE_POST_VAL,
    STATE_ARRAY_VAL,
    STATE_ARRAY_PRE_VAL,
    STATE_DIRECTIVE_ARG,
};

/* option value types converted to strings, sync
 * with the enum in lmc.h */
static const char *val_str[] =
{
    "LMC_VAL_T_UINT",
    "LMC_VAL_T_ARRAY",
    "LMC_VAL_T_STRING",
    "LMC_VAL_T_EXTRA",
    "LMC_VAL_T_FLAG",
};

/**
 * This is just an ugly hack to get the line number. 
 * It will re-loop the whole string starting from 'start'
 * until it reaches 'pos' and count each line break. The 
 * speed of this function is not prioritized since it is
 * ONLY run whenever a syntax error occurs.
 **/
static int
sgetline(const char *start, const char *pos)
{
    int ret = 1;
    const char *s = start;

    while (s < pos) {
        if (*s == '\n')
            ret ++;
        s ++;
    }

    return ret;
}

/** 
 * set the last_error member of lmc to an error
 * string
 **/
static void
set_error(lmc_parser_t *lmc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!lmc->last_error)
        if (!(lmc->last_error = malloc(LMC_MAX_ERR_LEN)))
            return;

    vsnprintf(lmc->last_error, LMC_MAX_ERR_LEN, fmt, ap);

    va_end(ap);
}

/** 
 * Create an lmc parser with root_object
 * as configuration target
 **/
lmc_parser_t*
lmc_create(void *root_object)
{
    lmc_parser_t *r;

    if (!(r = calloc(1, sizeof(struct lmc_parser))))
        return 0;

    r->root = root_object;
    return r;
}

void
lmc_destroy(lmc_parser_t *lmc)
{
    if (lmc->classes)
        free(lmc->classes);
    if (lmc->scopes)
        free(lmc->scopes);
    if (lmc->directives)
        free(lmc->directives);

    if (lmc->last_error)
        free(lmc->last_error);

    free(lmc);
}

/** 
 * Register a configuration file directive
 **/
M_CODE
lmc_add_directive(lmc_parser_t *lmc,
                  const struct lmc_directive *d)
{
    if (!(lmc->directives = realloc(lmc->directives,
                    sizeof(struct lmc_directive*)*(lmc->num_directives+1))))
        return M_OUT_OF_MEM;

    lmc->directives[lmc->num_directives] = (struct lmc_directive *)d;
    lmc->num_directives ++;

    return M_OK;
}


/** 
 * Register a configuration file class
 **/
M_CODE
lmc_add_class(lmc_parser_t *lmc,
              const struct lmc_class *cl)
{
    if (!(lmc->classes = realloc(lmc->classes,
                    sizeof(struct lmc_class*)*(lmc->num_classes+1))))
        return M_OUT_OF_MEM;

    lmc->classes[lmc->num_classes] = (struct lmc_class *)cl;
    lmc->num_classes ++;

    return M_OK;
}

/** 
 * Register a configuration file scope
 **/
M_CODE
lmc_add_scope(lmc_parser_t *lmc,
              const struct lmc_scope *scope)
{
    if (!(lmc->scopes = realloc(lmc->scopes,
                    sizeof(struct lmc_scope*)*(lmc->num_scopes+1))))
        return M_OUT_OF_MEM;

    lmc->scopes[lmc->num_scopes] = (struct lmc_scope *)scope;
    lmc->num_scopes ++;

    return M_OK;
}

/** 
 * Load the given file and parse it using lmc_parse()
 *
 * NOTE: Expects absolute path, or a path relative
 *       to the current working directory.
 **/
M_CODE
lmc_parse_file(lmc_parser_t *lmc,
               const char *filename)
{
    M_CODE   r;
    FILE    *fp;
    long     sz;
    char    *e, *p;

    if (!(fp = fopen(filename, "rb"))) {
        set_error(lmc, "error opening '%s': %s", filename, strerror(errno));
        return M_COULD_NOT_OPEN;
    }

    do {
        fseek(fp, 0, SEEK_END);

        if ((sz = ftell(fp)) > MAX_FILESIZE) {
            set_error(lmc, "file too big");
            r = M_TOO_BIG;
            break;
        }

        fseek(fp, 0, 0);
        /* Allocate a buffer capable of holding the whole file,
         * e will point to the end of the buffer and p
         * will point to the beginning. We allocate an extra byte
         * which we will set to '\0', letting us safely use strcmp()
         * and friends. */
        e = (p = malloc(sz+1)) + sz;
        if (!p) {
            r = M_OUT_OF_MEM;
            break;
        }

        *e = '\0';

        if (!fread(p, sizeof(char), sz, fp)) {
            set_error(lmc, "I/O error");
            r = M_IO_ERROR;
            break;
        }
        
        fclose(fp);

        char *s;
        if (!(s = strrchr(filename, '/')))
            s = (char*)filename;
        else s++;
        r = lmc_parse(lmc, s, p, sz);
        free(p);
        return r;
    } while (0);

    /* reach here if error before lmc_parse()
     * is called */
    fclose(fp);
    return r;
}

/**
 * lmc_parse()
 *
 * Parse the given buffer
 *
 * name - a name related to the buffer, for error output,
 *        most likely one would want to set this to the 
 *        filename from which the buffer comes
 *
 * TODO: If something goes wrong, there might be an object in 'o', 
 *       and if so we should call the destructor.
 **/
M_CODE
lmc_parse(lmc_parser_t *lmc,
          const char *name,
          const char *buf,
          size_t bufsz)
{
    char    *p, *e, *s;
    char    *t;
    int      state    = STATE_ROOT;
    int      extend   = 0;
    int      override = 0;
    int      copy     = 0;
    int      x, y;
    int      found;
    void    *o;
    M_CODE   status;
    char   **array = 0;
    int      array_sz = 0;
    int      scope = 0;

    if (lmc->num_scopes == 0 && lmc->num_directives == 0
            && lmc->num_classes == 0)
        return M_FAILED;

    /** 
     * NOTE: curr will be 0 when parsing scopes (that is, NOT classes),
     * and thus lmc_opt->offs MUST be absolute pointers for all scope 
     * options.
     **/
    struct lmc_class        *curr;
    struct lmc_opt          *curr_opt;
    struct lmc_directive    *curr_directive;

    for (p=(char*)buf, e=(char*)buf+bufsz; p<e; p++) {
        if (isspace(*p))
            continue;
        if (*p == '#') {
            for (p++; p<e && *p != '\n'; p++)
                ;
            continue;
        }
        if (*p == '/') {
            if (p+1 < e && *(p+1) == '*') {
                /* c-style commnet */
                for (p++; p<e; p++) {
                    if (*p == '*' && p+1 < e && *(p+1) == '/') {
                        p++;
                        break;
                    }
                }
                continue;
            } else
                goto uc;
        }

        switch (state) {
            case STATE_ROOT:
                /* In the 'root' state we check for:
                 * - directives
                 * - classes
                 * - scopes
                 **/

                /* find the end of this string token */
                for (s = p; s < e && (isalnum(*s) || *s == '_'); s++)
                    ;
                y = s-p;

                if (y == 0)
                    goto uc;

                if (y == 6 && strncmp(p, "extend", 6) == 0) {
                    p+=6;
                    if (*p != ':') {
                        set_error(lmc, "<%s:%d>: expected ':' after extend keyword",
                                     name, sgetline(buf, p));
                        goto error;
                    }

                    extend = 1;
                    continue;
                }
                if (y == 8 && strncmp(p, "override", 8) == 0) {
                    p+=8;
                    if (*p != ':') {
                        set_error(lmc, "<%s:%d>: expected ':' after override keyword",
                                name, sgetline(buf, p));
                        goto error;
                    }

                    extend = 1;
                    override = 1;
                    continue;
                }

                found = 0;
                for (x=0; x<lmc->num_classes; x++) {
                    if (strncmp(p, lmc->classes[x]->name, y) == 0) {
                        curr = (struct lmc_class *)lmc->classes[x];
                        state = STATE_PRE_NAME;
                        p+=y-1;
                        found = 1;
                        break;
                    }
                }
                if (found)
                    continue;

                for (x=0; x<lmc->num_scopes; x++) {
                    if (strncmp(p, lmc->scopes[x]->name, y) == 0) {
                        curr = (struct lmc_class *)lmc->scopes[x];
                        scope = 1;
                        state = STATE_PRE_OBJ;
                        p+=y-1;
                        found = 1;
                        o = 0; /* by setting o to 0, scopes can have absolute pointers
                                  as their "member offset" and the code below will still run
                                  properly */
                        break;
                    }
                }
                if (found)
                    continue;

                for (x=0; x<lmc->num_directives; x++) {
                    if (strncmp(p, lmc->directives[x]->name, y) == 0) {
                        p+=y;
                        found = 1;
                        state = STATE_DIRECTIVE_ARG;
                        curr_directive = lmc->directives[x];
                        break;
                    }
                }

                if (found)
                    continue;

                goto uc;

            case STATE_DIRECTIVE_ARG:
                /* reach here when we need to parse out an argument for a given directive */
                if (*p == '"') {
                    p++;
                    t = p+strcspn(p, "\"\n");
                    if (!*t || *t == '\n') {
                        set_error(lmc, "<%s:%d>: unterminated string constant",
                                name, sgetline(buf, p));
                        goto error;
                    }

                    *t = '\0';

                    if ((status = curr_directive->callback(lmc->root, p)) != M_OK) {
                        set_error(lmc, "<%s:%d>: %s \"%s\" failed\n",
                                name, sgetline(buf, p),
                                curr_directive->name, p);
                        goto error;
                    }

                    p = t;
                } else {
                    set_error(lmc, "<%s:%d>: expected a quoted argument for directive"
                                "'%s', found '%c'\n",
                            name, sgetline(buf, p),
                            curr_directive->name, *p);
                    goto error;
                }
                state = STATE_ROOT;
                break;

            case STATE_PRE_NAME:
                if (*p != '[') {
                    set_error(lmc, "<%s:%d>: expected '[', found '%c'",
                            name, sgetline(buf, p), *p);
                    goto error;
                }
                state = STATE_NAME;
                break;

            case STATE_POST_NAME:
                if (strncmp(p, "copy", 4) == 0) {
                    copy = 1;
                    p+=3;

                    state = STATE_NAME;
                    continue;
                }

            case STATE_END_NAME:
                if (*p == ']') {
                    state = STATE_PRE_OBJ;
                    continue;
                }
                goto uc;
                
            case STATE_NAME:
                if (*p == '"') {
                    p++;
                    t = p+strcspn(p, "\"\n");
                    if (!*t || *t == '\n') {
                        set_error(lmc, "<%s:%d>: unterminated string constant",
                                name, sgetline(buf, p));
                        goto error;
                    }

                    *t = '\0';

                    if (!(t-p)) {
                        set_error(lmc, "<%s:%d>: empty %s name",
                                name, sgetline(buf, p),
                                curr->name);
                        goto error;
                    }

                    if (!copy) {
                        /* Now that we have a name for the object, we can call
                         * the constructor. The constructor must expect two
                         * arguments, a pointer to the name of the object and
                         * its length. */
                        if (!extend)
                            o = curr->constructor_cb(p, (uint32_t)(t-p));
                        else {
                             /* if 'extend' is set to 1, then we are extending
                              * an already defined object, so we must look it
                              * up and modify it. */
                            if (!(o = curr->find_cb(lmc->root, p))) {
                                set_error(lmc, "<%s:%d>: undefined %s '%s'",
                                        name, sgetline(buf, p), curr->name, p);
                                goto error;
                            }
                            /* override might also be set, and if so then we
                             * should call the object's zero function */
                            if (override)
                                curr->zero_cb(o);

                        }
                        state = STATE_POST_NAME;
                    } else {
                        /* reach here if we want the name for an object
                         * after the 'copy' keyword */
                        void *cp;
                        if (!(cp = curr->find_cb(lmc->root, p))) {
                            set_error(lmc, "<%s:%d>: undefined %s '%s'",
                                    name,
                                    sgetline(buf, p), curr->name, p);
                            goto error;
                        }

                        if (curr->copy_cb(o, cp) != M_OK) {
                            set_error(lmc, "<%s:%d>: internal error while copying %s",
                                    name, sgetline(buf, p),
                                    curr->name);
                            goto error;
                        }

                        copy = 0;
                        state = STATE_END_NAME;
                    }

                    p = t;
                } else {
                    set_error(lmc, "<%s:%d>: expected quoted %s name, found '%c'",
                            name, sgetline(buf, p),
                            curr->name, *p);
                    goto error;
                }
                continue;

            case STATE_PRE_OBJ:
                if (*p == '{') {
                    state = STATE_OBJ;
                    continue;
                } else if (*p == ';') {
                    state = STATE_ROOT;
                    continue;
                }
                set_error(lmc, "<%s:%d>: expected '{' or ';', found '%c'",
                        name, sgetline(buf, p), *p);
                goto error;

            case STATE_OBJ:
                if (*p == '}') {
                    if (!scope) {
                        /* We reach here once we are done parsing this "object".
                         * So now we will add the object to the root object. */

                        /* if we extended an already defined object, there's
                         * nothing more to do here since the object is already
                         * added */
                        if (!extend) {
                            /* add the object to the root object using its add_cb 
                             * function */
                            if ((struct lmc_class *)(curr)->add_cb(lmc->root, o) != M_OK) {
                                /* out of mem? */
                                abort();
                            }
                        } else {
                            extend = 0;
                            override = 0;
                        }
                    } else {
                        /* we are done parsing a scope, so there's not much to do
                         * other than setting scope to 0 and continuing */
                        scope = 0;
                    }

                    o = 0;
                    state = STATE_ROOT;
                    continue;
                }

                x = 1;
                while (isalnum(*(p+x)) || *(p+x) == '-' || *(p+x) == '_')
                    x++;

                struct lmc_opt *opts =
                    (scope ? &((struct lmc_scope*)curr)->opts
                     : curr->opts);
                found = 0;
                for (y=0; opts[y].name; y++) {
                    if (strncmp(p, opts[y].name, x) == 0
                            && opts[y].name[x] == '\0') {
                        found = 1;
                        p += x-1;
                        curr_opt = &opts[y];
                        break;
                    }
                }

                if (!found) {
                    *(p+x) = '\0';
                    if (isalnum(*p)) {
                        set_error(lmc, "<%s:%d>: unknown option '%s'",
                                name, sgetline(buf, p), p);
                        goto error;
                    }
                    goto uc;
                }

                state = STATE_PRE_VAL;
                continue;

            case STATE_PRE_VAL:
                if (*p != '=') {
                    set_error(lmc, "<%s:%d>: expected '=', found '%c'",
                            name, sgetline(buf, p), *p);
                    goto error;
                }
                state = STATE_VAL;
                continue;

            case STATE_VAL:
                /* This is where we parse the actual value for each option, 
                 * some options expect strings, others integers, and some expect 
                 * arrays. */
                if (*p == '{') {
                    if (scope) {
                        set_error(lmc,
                                "<%s:%d>: arrays not supported by scopes",
                                name, sgetline(buf, p));
                        goto error;
                    } else if (curr_opt->type == LMC_VAL_T_ARRAY) {
                        state = STATE_ARRAY_VAL;
                        continue;
                    } else {
                        set_error(lmc,
                                "<%s:%d>: option '%s' expects a value of type %s",
                                name, sgetline(buf, p),
                                curr_opt->name, val_str[curr_opt->type]);
                        goto error;
                    }
                } else if (*p == '"') {
                    if (curr_opt->type == LMC_VAL_T_STRING
                            || curr_opt->type == LMC_VAL_T_EXTRA) {
                        p++;

                        t = p+strcspn(p, "\"\n");
                        if (!*t || *t == '\n') {
                            set_error(lmc,
                                    "<%s:%d>: unterminated string constant",
                                    name, sgetline(buf, p));
                            goto error;
                        }

                        *t = '\0';
                        if (curr_opt->type == LMC_VAL_T_EXTRA) {
                            if (curr_opt->value.set)
                                curr_opt->value.set(o, p);
                        } else {
                            if (curr_opt->value.offs)
                                *(char**)((char *)o+curr_opt->value.offs) = strdup(p);
                        }

                        p = t;
                    } else {
                        set_error(lmc, "<%s:%d>: option '%s' expects a value of type %s",
                                name, sgetline(buf, p),
                                curr_opt->name, val_str[curr_opt->type]);
                        goto error;
                    }
                } else if (isdigit(*p)) {
                    if (curr_opt->type == LMC_VAL_T_UINT) {
                        if (curr_opt->value.offs)
                            *(unsigned int*)((char *)o+curr_opt->value.offs) = atoi(p);
                    } else if (curr_opt->type == LMC_VAL_T_FLAG) {
                        if (scope) {
                            set_error(lmc,
                                    "<%s:%d>: flags not supported by scopes",
                                    name, sgetline(buf, p));
                            goto error;
                        }
                        if (atoi(p) && curr->flags_offs)
                            *(uint8_t*)((char *)o+curr->flags_offs) |= curr_opt->value.flag;
                    } else {
                        set_error(lmc, "<%s:%d>: option '%s' expects a value of type %s",
                                name, sgetline(buf, p),
                                curr_opt->name, val_str[curr_opt->type]);
                        goto error;
                    }
                    while (isdigit(*p))
                        p++;
                    p--;
                } else {
                    if (curr_opt->type == LMC_VAL_T_FLAG) {
                        if (scope) {
                            set_error(lmc,
                                    "<%s:%d>: flags not supported by scopes",
                                    name, sgetline(buf, p));
                            goto error;
                        }
                        if (strncasecmp(p, "true", 4) == 0) {
                            if (curr->flags_offs)
                                *(uint8_t*)((char *)o+curr->flags_offs) |= curr_opt->value.flag;
                            p+=3;
                        } else if (strncasecmp(p, "false", 5) == 0)
                            p+=4; /* TODO: set flag to 0 */
                        else {
                            set_error(lmc, "<%s:%d>: expected %s, found '%c'",
                                    name, sgetline(buf, p),
                                    val_str[curr_opt->type], *p);
                            goto error;
                        }
                    } else {
                        set_error(lmc, "<%s:%d>: expected %s, found '%c'",
                                name, sgetline(buf, p),
                                val_str[curr_opt->type], *p);
                        goto error;
                    }
                }
                state = STATE_POST_VAL;
                break;

            case STATE_POST_VAL:
                if (*p != ';') {
                    set_error(lmc, "<%s:%d>: expected ';', found '%c'",
                            name, sgetline(buf, p), *p);
                    goto error;
                }
                state = STATE_OBJ;
                continue;

            case STATE_ARRAY_PRE_VAL:
                /* state between all values in an array */
                if (*p == '}') {
                    state = STATE_POST_VAL;

                    /* we are done parsing the array, time to send the 
                     * built array to the callback function for this
                     * variable */
                    if (curr_opt->value.array_set)
                        curr_opt->value.array_set(o, array, array_sz);
                    array = 0;
                    array_sz = 0;
                }
                else if (*p == ',')
                    state = STATE_ARRAY_VAL;
                else
                    goto uc;
                continue;

            case STATE_ARRAY_VAL:
                if (*p != '"')
                    goto uc;

                p++;
                t = p+strcspn(p, "\"\n");
                if (!*t || *t == '\n') {
                    set_error(lmc, "<%s:%d>: unterminated string constant",
                            name, sgetline(buf, p));
                    goto error;
                }

                *t = '\0';

                if (curr_opt->value.array_set) {
                    /* current value in the array will start at p and reach to t */
                    if (!(array = realloc(array, (array_sz+1)*sizeof(char*))))
                        return M_OUT_OF_MEM;
                    if (!(array[array_sz] = malloc((t-p)+1)))
                        return M_OUT_OF_MEM;

                    memcpy(array[array_sz], p, (t-p)+1);
                    array_sz ++;
                }

                p = t;
                state = STATE_ARRAY_PRE_VAL;
                continue;

            default:
                set_error(lmc, "internal error, lost scope");
                goto error;
        }
    }

    if (state != STATE_ROOT) {
        set_error(lmc, "<%s>: unexpected end of file", name);
        goto error;
    }

    if (array) free(array);
    return M_OK;

uc:
    set_error(lmc, "<%s:%d>: unexpected char '%c'", name, sgetline(buf, p), *p);

error:
    if (array) free(array);
    return M_FAILED;
}

