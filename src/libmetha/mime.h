/*-
 * mime.h
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

#ifndef _LM_MIME__H_
#define _LM_MIME__H_

#include <stdint.h>
#include "filetype.h"

/** 
 * the hash table will store pointers to the mime types you
 * add to it, but it will neither copy the contents nor 
 * free them when done
 **/

/* num mime type categories, adjust mime.c accordingly */
#define LM_MT_NUM_CATS 5

typedef uint8_t mime_t;

/** 
 * node type used for list form table,
 * before the table have been 
 * converted to a constant-sized hash table
 **/
typedef struct mimetb_nl {
    FT_ID id; /* corresponding filetype */
    mime_t mime;
    const char *str;
} mimetb_nl_t;

/** 
 * node type used in the hash table, 
 * each field in the hash table ends with a 
 * linked list of these nodes
 **/
typedef struct mimetb_nh {
    FT_ID id; /* corresponding filetype */
    const char *str;
    struct mimetb_nh *next;
} mimetb_nh_t;

typedef struct mimetb {
    void **tbl[LM_MT_NUM_CATS]; /* one list for each mime type, see mime.c */
    uint8_t listsz[LM_MT_NUM_CATS];
} mimetb_t;

mime_t  lm_mime_hash(const char *s);
M_CODE  lm_mimetb_init(mimetb_t *t);
FT_ID   lm_mimetb_lookup(mimetb_t *t, const char *mime);
void    lm_mimetb_uninit(mimetb_t *t);
M_CODE  lm_mimetb_add(mimetb_t *t, const char *mime, FT_ID id);

#endif

