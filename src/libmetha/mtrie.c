/*-
 * mtrie.c
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
 * Based on ptrindex from methabot/0.x series,
 * uses much less RAM and performs faster.
 *
 * The table is case-insensitive and expects URL-encoded 
 * strings. It might perform pretty weird if you use it with 
 * strings not url-encoded, since this table uses only 6 bits 
 * for each characters. Bit 7 and 8 (most significant) are used 
 * as magic numbers. Bit 7 signifies a match, bit 8 signifies 
 * multi-char connections.
 *
 * It does, however, not store any data associated with 
 * added strings anymore. Matches are simply implied 
 * by a logical OR of 0x40 on the magic number, and an
 * integer value of 0 (mtrie_tryadd()) is returned once this is found.
 * This makes the lookup table suitable for the URL 
 * cache in libmetha, since no metadata is really needed.
 *
 * This table is not used for mime types and file extensions 
 * anymore, because those suffice better in tiny hash tables.
 *
 * http://bithack.se/projects/methabot/docs/mtrie.html
 **/

#include "mtrie.h"
#include <string.h>
#include <assert.h>

/*#define MTRIE_DEBUG*/

#ifdef MTRIE_DEBUG
#include <stdio.h>
#define _DEBUG(x, ...) fprintf(stderr, "* " x "\n", __VA_ARGS__)
static const char decodetbl[] = " !_#$%&'()*+,-./0123456789:;<=>?`abcdefghijklmnopqrstuvwxyz{|}~";
#else
#define _DEBUG(x, ...)
#endif

#ifdef inl_
# undef inl_
#endif

#define inl_   static inline
typedef struct mtrie_node   NODE;
typedef struct mtrie_br_pos BRANCH;
typedef struct mtrie_leaf   LEAF;
typedef struct mtrie_conn   CONN;

static void br_free(NODE *n);
static void node_free(NODE *n);

mtrie_t *
mtrie_create(void)
{
    mtrie_t* p;

    if ((!(p = calloc(1,sizeof(mtrie_t)))))
        return 0;

    return p;
}

void
mtrie_destroy(mtrie_t* p)
{
    mtrie_cleanup(p);
    free(p);
}

inl_ NODE*
mtrie_next(NODE *n, BRANCH *p, char count, char c)
{
    char top  = count-1;
    int  x;

    if (!p)
        goto add;
    if (c < p[0].c) {
        x = 0;
        goto add;
    }
    if (c > p[(int)top].c) {
        x = count;
        goto add;
    }
#if 0
    if (count < 4) {
#endif
        for (x=0; x<count; x++) {
            if (p[x].c == c)
                return &p[x].node;
            else if (p[x].c > c) {
                /* insert this one at x */
                goto add;
            }
        }
        /* temporarily disable optimization code.
         * TODO: fix the code so it doesn't crash,
         * we need a faster lookup. */
#if 0
    } else {
        if (!(y = (c - p[0].c)))
            return &p[0].node;
        /* estimate where in the array this char is */
        x = (y/((p[top].c - p[0].c)/count));
        if (x >= count)
            x = count-1;
        if (p[x].c > c) {
            /* loop downwards */
            for (x--; x>=0; x--) {
                if (p[x].c == c)
                    return &p[x].node;
                if (p[x].c < c) {
                    x++;
                    goto add;
                }
            }
        } else if (p[x].c < c) {
            /* loop upwards */
            for (x++; x<count; x++) {
                if (p[x].c == c)
                    return &p[x].node;
                if (p[x].c > c)
                    goto add;
            }
        } else
            return &p[x].node;
    }
#endif

add:
    _DEBUG("branch:(%p) realloc(), new address is:", p);
    p = realloc(p, sizeof(struct mtrie_br_pos)*(count+1));
    _DEBUG("-- %p", p);
    if (x != count)
        memmove(p+x+1, p+x, (count-x)*sizeof(struct mtrie_br_pos));
    p[x].c = c;
    p[x].node.magic = 0;
    p[x].node.next = 0;
    n->next = p;
    n->magic++;
    _DEBUG("branch:(%p) added char '%c' (%hhd)", p, decodetbl[c], c);

    return &p[x].node;
}

static void
node_free(NODE *n)
{
    if (n->magic & 0x80) {
        if (n->magic & 0x3f)
            free(n->next); /* free leaf */
        else {
            br_free(&((CONN*)n->next)->node);
            free(n->next);
        }
    } else 
        br_free(n);
}

static void
br_free(NODE *n)
{
    int x;
    int v = n->magic & 0x3f;
    for (x=0; x<v; x++)
        node_free(&n->next[x].node);
    free(n->next);
}

/**
 * Clean up the whole table, free all allocated buffers
 **/
void
mtrie_cleanup(mtrie_t *p)
{
    NODE *n;
    
    n = &p->entry;
    node_free(n);
}

/** 
 * Add a URL to the mtrie-object.
 *
 * This is a combined lookup/add function. A return 
 * value of 0 tells us that the URL was not added, 
 * because it had (presumably -- disregarding error cases)
 * been added to the table before. A return value of 1 
 * tells us the URL was successfully added to the table, 
 * hence it couldn't have been added before.
 *
 * See http://bithack.se/projects/methabot/docs/mtrie.html
 **/
int
mtrie_tryadd(mtrie_t *p, url_t *url)
{
    NODE *n;
    uint8_t       v; /* current char, converted through MTRIE_OFFS() */
    uint_fast16_t x, y;
    const char *s;
    const char *e;
    uint16_t   sz;

    /*
    s = url->str;
    e = url->str+url->sz;
    */
    s = url->str+url->host_o;
    e = url->str+url->sz;
    n = &p->entry;

cont:for (;;) {
        if (s == e) {
            if (n->magic & 0x40)
                return 0;
            n->magic |= 0x40;
            return 1;
        }

        v = n->magic & 0x3f;
        if (n->magic & 0x80) {
            /* this is a multi-char connection between one 
             * leaf or many nodes */
            char     *s2;
            void     *next = (void*)n->next;
            int       leaf; /* leaf or conn */

            if (v) {
                /* type is leaf */
                s2 = (char*)&((LEAF *)(n->next))->s;
                sz = ((LEAF *)(n->next))->sz;
                leaf = 1;
            } else {
                /* type is conn */
                s2 = (char*)&((CONN *)(n->next))->s;
                sz = ((CONN *)(n->next))->sz;
                leaf = 0;
            }
            y = (sz > e-s)?e-s:sz;
            for (x=0; x<y; x++) {
                register uint8_t c1 = MTRIE_OFFS(*(s+x));
                register uint8_t c2 = (*(s2+x) & 0x3f);
                if (c1 == c2)
                    continue;
                uint8_t magic;
                BRANCH* br = malloc(sizeof(BRANCH)*2);
                if (!x) {
                    /* it was the first char that didnt match */
                    n->magic = (n->magic & 0x40) + 2;
                    n->next  = br;
    /*                x = 1;*/
                } else {
                    CONN *new = (CONN*)(n->next = malloc(sizeof(CONN)+x));
                    memcpy(new->s, s2, x);
                    new->sz = x;
                    new->node.magic = 2;
                    new->node.next = br;
                    n->magic = (n->magic & 0x40) | 0x80;
                }
                /* split the leaf/conn */
                if (leaf) {
                    LEAF *leaf = (LEAF*)next;
                    _DEBUG("leaf:(%p) split at char '%c', leaf-pos %d",
                            leaf,
                            decodetbl[*(s2+x)], x);
                    if (leaf->sz == 1) {
                        _DEBUG("leaf:(%p) new size was %d, destroyed",
                                leaf, -1);
                        free(leaf);
                        magic = 0x40;
                        next = 0;
                    } else {
                        magic = (0x81 | (*(s2+x) & 0x40));
                        /* shrink the leaf... */
                        _DEBUG("leaf:(%p) new size = %d, old size = %d",
                                leaf,
                                leaf->sz-x-1, leaf->sz);
                        leaf->sz -= x+1;
                        if (leaf->sz == 0) {
                            free(leaf);
                            magic = 0x40;
                            next = 0;
                        } else {
                            memmove(leaf->s, leaf->s+x+1, leaf->sz);
                            leaf = realloc(leaf, sizeof(LEAF)+leaf->sz);
                            next = (void*)leaf;
                        }
                    }
                } else {
                    magic = (0x80 | (*(s2+x) & 0x40));
                    /* shrink the conn... */
                    CONN *conn = (CONN*)next;
                    conn->sz -= x+1;
                    memmove(conn->s, conn->s+x+1, conn->sz);
                    conn = realloc(conn, sizeof(CONN)+conn->sz);
                    next = (void*)conn;
                }
                /* sort the two nodes */
                v = (c1 > c2);
                (*(br+v)).c = c1;
                (*(br+v)).node.magic = 0;
                (*(br+v)).node.next  = 0;
                n = &((*(br+v)).node);
                (*(br+!v)).c = c2;
                (*(br+!v)).node.magic = magic;
                (*(br+!v)).node.next  = next;
                _DEBUG("branch:(%p) new with [0] = '%c', [1] = '%c'",
                        br, decodetbl[br[0].c], decodetbl[br[1].c]);
                s+=x+1;
                goto cont;
            }
            if (leaf) {
                if (e-s > sz) {
                    /* expand leaf */
                    LEAF *leaf = (LEAF*)n->next;
                    _DEBUG("leaf:(%p) expanding to size: "
                            "%d, with, '%s', new address:",
                            leaf, e-s, s);
                    leaf = realloc(leaf, sizeof(leaf)+(e-s));
                    _DEBUG("-- %p", leaf);
                    leaf->sz = y = e-s;
                    leaf->s[x-1] |= 0x40/* | MTRIE_OFFS(*(s+x))*/;
                    for (;x<y;x++)
                        leaf->s[x] = MTRIE_OFFS(*(s+x));
                    n->next = (BRANCH*)leaf;
                    return 1;
                } else if (x < sz) {
                    if (*(s2+x-1) & 0x40)
                        return 0;
                    *(s2+x-1) |= 0x40;
                    return 1;
                }
                return 0;
            }
            /* for conn */
            if (x == e-s) {
                if (*(s2+x-1) & 0x40)
                    return 0;
                *(s2+x-1) |= 0x40;
                return 1;
            }
            /* the string matched the complete conn */
            s+=x;
            n = mtrie_next(&((CONN*)n->next)->node, ((CONN*)n->next)->node.next,
                    ((CONN*)n->next)->node.magic & 0x3f, MTRIE_OFFS(*s));
            s++;
            continue;
        } else if (!v) {
            /* node branch is empty, add this new as a leaf */
            n->magic |= 0x81;
            sz = e-s; /* size */
            LEAF *leaf = (LEAF*)(n->next = malloc(sizeof(LEAF)+sz));

            _DEBUG("leaf:(%p) new with '%s'", leaf, s);

            leaf->sz = sz;
            while (sz > 0) {
                sz--;
                leaf->s[sz] = MTRIE_OFFS(*(s+sz));
            }
            return 1;
        }
        n = mtrie_next(n, n->next, v, MTRIE_OFFS(*s));
        s++;
    }
    return 0;
}

