/*-
 * filetype.h
 * This file is part of libmetha
 *
 * Copyright (c) 2008, Emil Romanus <emil.romanus@gmail.com>
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

#ifndef _LM_FILETYPE__H_
#define _LM_FILETYPE__H_

#include <stdint.h>
#include "errors.h"
#include "wfunction.h"
#include "umex.h"
#include "config.h"

#define FT_FLAG_HAS_PARSER    1
#define FT_FLAG_HAS_HANDLER   2
#define FT_FLAG_IGNORE_HOST   4

#define FT_FLAG_ISSET(ft, x) ((ft)->flags & (x))
#define FT_ID_NULL ((FT_ID)-1)

#if HAVE_BUILTIN_ATOMIC == 1
#define lm_filetype_counter_inc(ft) __sync_add_and_fetch(&(ft)->counter, 1)
#else
#include <pthread.h>
#define lm_filetype_counter_inc(ft) {\
    pthread_mutex_lock(&(ft)->counter_lk); \
    ft->counter++;\
    pthread_mutex_unlock(&(ft)->counter_lk); \
    }
#endif

/* shouldn't be called while running, and if it is it
 * should be an atomic operation on most architures i
 * know of anyway (since it's a 4-byte variable) */
#define lm_filetype_counter_reset(ft) (ft)->counter = 1

typedef uint8_t FT_ID;

typedef struct parser_chain {
    wfunction_t   **parsers;
    unsigned int    num_parsers;
} parser_chain_t;

typedef struct filetype {
    char    *name;
    uint8_t  id; /* this is set up by lmetha_prepare() */

    /** 
     * extensions, mimetypes and attributes are represented 
     * as arrays in configuration files. Their set-functions
     * (lm_filetype_*_set()) expect a pointer array.
     * XXX: Note that the *_set() functions will TAKE the 
     *      array given, and not make a copy of it.
     **/
    char   **extensions;
    int      e_count;
    char   **mimetypes;
    int      m_count;
    char   **attributes;
    int      attr_count;

    umex_t *expr;

    /* parser_str identifies the parser_chain, and will
     * contain the full string as given to the 'parser'
     * filetype option in configuration files. */
    char                *parser_str;
    parser_chain_t       parser_chain;

    union {
        char           *name;
        struct crawler *ptr;
    } switch_to;

    union {
        wfunction_t *wf;
        char        *name;
    } handler;

    /* counter for how many URLs that matches this filetype */
    volatile uint32_t    counter;
#if HAVE_BUILTIN_ATOMIC == 0
    pthread_mutex_t      counter_lk;
#endif
    uint8_t              flags;
} filetype_t;

filetype_t *lm_filetype_create(const char *name, uint32_t nlen);
void    lm_filetype_destroy(filetype_t *ft);
M_CODE  lm_filetype_add_extension(filetype_t *ft, const char *name);
M_CODE  lm_filetype_add_mimetype(filetype_t *ft, const char *name);
M_CODE  lm_filetype_add_parser(filetype_t *ft, wfunction_t *p);
M_CODE  lm_filetype_set_extensions(filetype_t *ft, char **extensions, int num_extensions);
M_CODE  lm_filetype_set_attributes(filetype_t *ft, char **attributes, int num_attributes);
M_CODE  lm_filetype_set_mimetypes(filetype_t *ft, char **mimetypes, int num_mimetypes);
M_CODE  lm_filetype_set_expr(filetype_t *ft, const char *expr);
void    lm_filetype_clear(filetype_t *ft);
M_CODE  lm_filetype_dup(filetype_t *dest, filetype_t *source);

#endif

