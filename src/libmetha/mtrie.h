/*-
 * mtrie.h
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

/* see comments in mtrie.c */

#ifndef _MTRIE__H_
#define _MTRIE__H_

#include <stdlib.h>
#include <stdint.h>

#include "url.h"

#define MTRIE_OFFS(x)              \
    (x=='_'?2:((x-32)&0x40?x-64:x-32))

struct mtrie_leaf {
    uint16_t sz;
    char     s[];
};

struct mtrie_node {
    char magic;
    struct mtrie_br_pos *next;
};

/* used for connecting two nodes by more than one character */
struct mtrie_conn {
    uint16_t sz;
    struct mtrie_node node;
    char     s[];
};

struct mtrie_br_pos {
    char c;
    struct mtrie_node node;
};

typedef struct mtrie {
    struct mtrie_node entry;
} mtrie_t;

mtrie_t* mtrie_create(void);
void   mtrie_destroy(mtrie_t *p);
void   mtrie_cleanup(mtrie_t *p);
int    mtrie_tryadd(mtrie_t *p, url_t *url);

#endif
