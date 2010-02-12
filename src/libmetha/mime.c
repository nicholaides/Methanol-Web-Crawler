/*-
 * mime.c
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

#include "mime.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* switch over to reference-by-hash if 
 * size of mimetb_t->tbl > MT_HASH_LIST_LIMIT,
 * elsewise use a list to find the mime*/
#define MT_HASH_LIST_LIMIT 4

/* XXX if you add one of these, make sure to 
 * add +1 to LM_MT_NUM_CATS (mime.h) */
#define MT_OTHERS        0
#define MT_AUDIO         (1<<5)
#define MT_APPLICATION   (2<<5)
#define MT_VIDEO         (3<<5)
#define MT_TEXT          (4<<5)
/* a mime_t is 8 bits. The first 5 bits are 
 * used for the type, and the 3 (most significant) 
 * bits are used for the "category". This 
 * currently limits our amount of categories
 * to 8, but it can easily be changed later
 * if necessary. */

#define MT_HASH_COUNT         29
#define MT_HASH_MASK          31
#define MT_HASH_CAT_BIT_OFFS  5
/** 
 * Create a hash from the given mimetype.
 * Format should be "mime/type", for example:
 * "text/html"
 **/
mime_t
lm_mime_hash(const char *s)
{
    const char *p=s;
    uint32_t type = MT_OTHERS;
    mime_t r = 0;

    switch (*p) {
        case 'a':
            p++;
            if (tolower(*p) == 'u') {
                if (strncasecmp(p+1, "dio", 3) == 0) {
                    type = MT_AUDIO;
                    p+=3;
                }
            } else if (tolower(*p) == 'p') {
                if (strncasecmp(p+1, "plication", 9) == 0) {
                    type = MT_APPLICATION;
                    p+=9;
                }
            }
            break;
        case 'v':
            p++;
            if (strncasecmp(p, "ideo", 4) == 0) {
                type = MT_VIDEO;
                p+=4;
            }
            break;
        case 't':
            p++;
            if (strncasecmp(p, "ext", 3) == 0) {
                type = MT_TEXT;
                p+=3;
            }
            break;
        default:
            break;
    }

    if (*p == '/')
        p++;
    else {
        if (!*p)
            return type;
        else {
            if ((p = strchr(p, '/')))
                p++;
            else
                return type;
        }
    }

    s = p;
    while (*p)
        r = (r<<2) + *p++;
    return type | (((r & MT_HASH_MASK)%MT_HASH_COUNT)+1);
}

M_CODE
lm_mimetb_init(mimetb_t *t)
{
    memset(t, 0, sizeof(mimetb_t));
    return M_OK;
}

void
lm_mimetb_uninit(mimetb_t *t)
{
    int x,y;
    for (x=0; x<LM_MT_NUM_CATS; x++) {
        if (t->tbl[x]) {
            if (t->listsz[x] >= MT_HASH_LIST_LIMIT) {
                mimetb_nh_t **list = (mimetb_nh_t**)t->tbl[x];
                for (y=0; y<MT_HASH_COUNT; y++) {
                    mimetb_nh_t *curr = list[y];
                    void *qwerty;
                    if (curr) {
                        do {
                            qwerty = curr->next;
                            free(curr);
                        } while ((curr = qwerty));
                    }
                }
            } else {
                /*for (y=0; y<t->listsz[x]; y++)
                    free(((mimetb_nl_t*)t->tbl[x])[y]);*/
            }
            free(t->tbl[x]);
        }
    }
    return;
}

/** 
 * Add a mime type to the table, 
 * id is the value to associate with the
 * mime type
 **/
M_CODE
lm_mimetb_add(mimetb_t *t, const char *mime, FT_ID id)
{
    mime_t m = lm_mime_hash(mime);
    int cat = (m & ~MT_HASH_MASK) >> MT_HASH_CAT_BIT_OFFS;
    int offs = m & MT_HASH_MASK;

    if (t->listsz[cat] == MT_HASH_LIST_LIMIT) {
        /* type is hash table */
        mimetb_nh_t *this;
        h_add:
        this = malloc(sizeof(mimetb_nh_t));
        if (!this)
            return M_OUT_OF_MEM;
        this->id   = id;
        this->str  = mime;
        this->next = 0;

        if (t->tbl[cat][offs]) {
            mimetb_nh_t *n=t->tbl[cat][offs];
            while (n->next)
                n=n->next;
            /* add this mime type to the list */
            n->next = this;
        } else
            t->tbl[cat][offs] = this;
    } else {
        mimetb_nl_t *this;
        /* type is list */
        if (t->listsz[cat]+1 == MT_HASH_LIST_LIMIT) {
            /* convert to hash table */
            int x;
            this = (mimetb_nl_t*)t->tbl[cat];
            if (!(t->tbl[cat] = calloc(1,sizeof(mimetb_nh_t*)*MT_HASH_COUNT)))
                return M_OUT_OF_MEM;

            t->listsz[cat]++; /* add one for listsz to exceed MT_HASH_LIST_LIMIT */

            for (x=0; x<t->listsz[cat]-1; x++)
                lm_mimetb_add(t, this[x].str, this[x].id);

            free(this);

            goto h_add;
        }
        if (!t->tbl[cat]) {
            if (!(this = (mimetb_nl_t*)(t->tbl[cat] = malloc(sizeof(mimetb_nl_t)))))
                return M_OUT_OF_MEM;
        } else {
            this = (mimetb_nl_t*)(t->tbl[cat] = realloc(t->tbl[cat], (t->listsz[cat]+1)*sizeof(mimetb_nl_t)));
            if (!this)
                return M_OUT_OF_MEM;
            this += t->listsz[cat];
        }
        t->listsz[cat]++;
        this->str = mime;
        this->id = id;
        this->mime = m;
    }

    return M_OK;
}

/** 
 * Find the mime, return the corresponding filetype ID
 **/
FT_ID
lm_mimetb_lookup(mimetb_t *t, const char *mime)
{
    mime_t m = lm_mime_hash(mime);
    int c = (m & ~MT_HASH_MASK) >> MT_HASH_CAT_BIT_OFFS; /* category */
    int o = (m & MT_HASH_MASK); /* offset in hash table */

    if (t->listsz[c] == MT_HASH_LIST_LIMIT) {
        /* hash table search */
        mimetb_nh_t *n = t->tbl[c][o];
        do {
            if (strcmp(mime, n->str) == 0)
                return n->id;
        } while ((n = n->next));
    } else {
        /* list form search */
        if (!t->tbl[c])
            return FT_ID_NULL;
        int x;
        for (x=0; x<t->listsz[c]; x++)
            if (((mimetb_nl_t*)(t->tbl[c]))[x].mime == m && strcmp(((mimetb_nl_t*)(t->tbl[c]))[x].str, mime) == 0)
                return ((mimetb_nl_t*)(t->tbl[c]))[x].id;
    }

    return FT_ID_NULL;
}

