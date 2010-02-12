/*-
 * utable.h
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

/** 
 * URL table and list structures used by the URL engine (urlengine.c)
 **/

#ifndef _METHA_UTABLE__H_
#define _METHA_UTABLE__H_

#include <stddef.h>
#include "url.h"

#define UTABLE_DEFAULT_PREALLOC 2
#define ULIST_DEFAULT_PREALLOC  16

typedef struct ulist {
    void *private;
    size_t cap;
    size_t sz;
    url_t *row;
} ulist_t;

typedef struct utable {
    size_t      cap;
    size_t      sz;
    ulist_t    *row;
} utable_t;

M_CODE lm_utable_init(utable_t *tb);
void   lm_utable_uninit(utable_t *tb);
M_CODE lm_utable_dec(utable_t *tb);
M_CODE lm_utable_inc(utable_t *tb);
ulist_t *lm_utable_top(utable_t *tb);

M_CODE lm_ulist_init(ulist_t *ul, unsigned int allocsz);
void   lm_ulist_uninit(ulist_t *ul);
url_t *lm_ulist_pop(ulist_t *ul);
url_t *lm_ulist_inc(ulist_t *ul);

#define lm_ulist_row(l, x) (&((l)->row[x]))
#define lm_ulist_dec(l) (l)->sz--
#define lm_ulist_top(l) &((l)->row[((l)->sz-1)])

#endif

