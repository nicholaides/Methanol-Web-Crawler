/*-
 * crawler.h
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

#ifndef _CRAWLER__H_
#define _CRAWLER__H_

#include "url.h"
#include "ftindex.h"
#include "io.h"
#include "urlengine.h"

#define lm_crawler_flag_isset(x,y) \
          ((x)->flags & (y))
#define lm_crawler_flag_set(x, y) \
          (x)->flags |= y
#define lm_crawler_flag_unset(x, y) \
          (x)->flags &= ~y

/* crawler flags */
enum {
    LM_CRFLAG_EPEEK          = 1, /* external peek */
    LM_CRFLAG_EXTERNAL       = 1<<1, 
    LM_CRFLAG_PREPARED       = 1<<2, 
    LM_CRFLAG_SPREAD_WORKERS = 1<<3, 
    LM_CRFLAG_JAIL           = 1<<4, 
    LM_CRFLAG_ROBOTSTXT      = 1<<5, 
};

typedef struct crawler {
    char         *name;
    ftindex_t     ftindex;

    char        **filetypes;
    int           num_filetypes;

    url_t         jail_url;
    
    union {
        char         *name;
        wfunction_t  *wf;
    } default_handler;

    /* options */
    unsigned int  depth_limit;
    unsigned int  peek_limit;   /* when external peek is used */
    char         *init;         /* init function for this crawler */
    union {
        char       *name;
        filetype_t *ptr;
    } initial_filetype; /* type of the initial URLs */
    uint8_t       flags;
} crawler_t;

crawler_t *lm_crawler_create(const char *name, uint32_t nlen);
void       lm_crawler_destroy(crawler_t *c);
M_CODE     lm_crawler_add_filetype(crawler_t *cr, const char *name);
M_CODE     lm_crawler_set_filetypes(crawler_t *cr, char **filetypes, int num_filetypes);
void       lm_crawler_clear(crawler_t *c);
M_CODE     lm_crawler_dup(crawler_t *dest, crawler_t *source);

#endif

