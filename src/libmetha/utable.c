/*-
 * utable.c
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

#include "utable.h"
#include <stdlib.h>

/** 
 * Initialize an already allocated utable
 **/
M_CODE
lm_utable_init(utable_t *tb)
{
    int x;

    if (!(tb->row = malloc(UTABLE_DEFAULT_PREALLOC*sizeof(ulist_t))))
        return M_OUT_OF_MEM;

    for (x=0; x<UTABLE_DEFAULT_PREALLOC; x++)
        if (lm_ulist_init(&tb->row[x], ULIST_DEFAULT_PREALLOC) != M_OK)
            return M_OUT_OF_MEM;

    tb->cap = UTABLE_DEFAULT_PREALLOC;
    tb->sz = 1;

    return M_OK;
}

void
lm_utable_uninit(utable_t *tb)
{
    int x;
    for (x=0; x<tb->cap; x++)
        lm_ulist_uninit(&tb->row[x]);
    free(tb->row);
    tb->sz = 1;
    tb->cap = 0;
}

/** 
 * Return a pointer to the list at the top
 * of the table.
 **/
ulist_t *
lm_utable_top(utable_t *tb)
{
    if (tb->sz == 0)
        return 0;
    return &tb->row[tb->sz-1];
}

/** 
 * Decrease the size of the table by removing
 * the top list
 **/
M_CODE
lm_utable_dec(utable_t *tb)
{
    if (tb->sz == 0)
        return M_FAILED;
    tb->row[tb->sz-1].sz = 0;
    tb->sz--;
    return M_OK;
}

/** 
 * Increase the size of the table
 **/
M_CODE
lm_utable_inc(utable_t *tb)
{
    int x;
    if (tb->sz >= tb->cap) {
        /* need realloc */
        tb->cap *= 2;
        if (!(tb->row = realloc(tb->row, tb->cap*sizeof(ulist_t))))
            return M_OUT_OF_MEM;
        for (x=tb->sz; x<tb->cap; x++)
            lm_ulist_init(&tb->row[x], ULIST_DEFAULT_PREALLOC);
    } else
        tb->row[tb->sz].private = 0;

    tb->sz++;
    return M_OK;
}

/** 
 * Put ulist_t-related functions below this line
 * --------------------------------------------------------
 **/

M_CODE
lm_ulist_init(ulist_t *ul, unsigned int allocsz)
{
    int x;

    if (allocsz) {
        if (!(ul->row = malloc(allocsz*sizeof(url_t))))
            return M_OUT_OF_MEM;
        for (x=0; x<allocsz; x++)
            lm_url_init(&ul->row[x]);
    }

    ul->cap = allocsz;
    ul->sz = 0;
    ul->private = 0;

    return M_OK;
}

void
lm_ulist_uninit(ulist_t *ul)
{
    int x;
    if (ul->cap) {
        for (x=0; x<ul->cap; x++) {
            lm_url_uninit(&ul->row[x]);
        }
        free(ul->row);
    }
    ul->cap = 0;
    ul->sz = 0;
}

url_t *
lm_ulist_pop(ulist_t *ul)
{
    url_t *url;
    /* TODO: resize downwards if necessary? */
    do {
        if (ul->sz == 0)
            return 0;
        ul->sz --;
        url = &ul->row[ul->sz];
    } while (!url->sz);
    /* loop to skip nullified/empty urls */

    return url;
}

/** 
 * Increase the size of the list and return a pointer
 * to the URL on the top, so that we can set it externally
 **/
url_t *
lm_ulist_inc(ulist_t *ul)
{
    int x;

    if (!ul->cap) {
        ul->sz = 0;
        ul->cap = ULIST_DEFAULT_PREALLOC;
        if (!(ul->row = malloc(ULIST_DEFAULT_PREALLOC*sizeof(url_t)))) {
            return 0;
        }
        for (x=0; x<ULIST_DEFAULT_PREALLOC; x++)
            lm_url_init(&ul->row[x]);
    }
    if (ul->sz >= ul->cap) {
        /* resize is necessary */
        ul->cap <<= 1;
        if (!(ul->row = realloc(ul->row, ul->cap*sizeof(url_t))))
            return 0;
        for (x=ul->sz; x<ul->cap; x++)
            lm_url_init(&ul->row[x]);
    }

    ul->sz++;
    return &ul->row[ul->sz-1];
}

