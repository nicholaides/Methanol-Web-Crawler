/*-
 * nolp.c
 * This file is part of libmethaconfig
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

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "nolp.h"

/** 
 * create a nolp object. nolps are used to parse the 
 * methanol protocol, it's a very simple set of 
 * functions to bind callback functions will protocol
 * "commands"
 **/
nolp_t*
nolp_create(struct nolp_fn *fn, int sock)
{
    nolp_t *r;

    if (!(r = malloc(sizeof(nolp_t)))
            || !(r->buf = malloc(NOLP_DEFAULT_BUFSZ)))
        return 0;

    r->state = NOLP_CMD;
    r->sz = 0;
    r->fd = sock;
    r->cap = NOLP_DEFAULT_BUFSZ;
    r->fn = fn;

    return r;
}

/** 
 * Expect a raw line, send the full line to the
 * callback function once found.
 **/
int
nolp_expect_line(nolp_t *no,
                 int (*cb)(void*, char *buf, int size))
{
    no->state = NOLP_LINE;
    no->next_cb = cb;
    return 0;
}

/**
 * buffer data from the given socket, if state is 
 * NOLP_CMD, then callback functions will be invoked
 * whenever a newline-character is found and the first
 * word in the buffer matches a command callback.
 *
 * return 0 on success, -1 on error
 **/
int
nolp_recv(nolp_t *no)
{
    int   sz;
    int   x;
    int   last = no->sz;
    int   sock = no->fd;
    int   rerun;
    char *p;
    char *s;

    if (no->state == NOLP_CMD && no->sz >= no->cap) {
        no->cap = no->cap+NOLP_DEFAULT_BUFSZ;
        no->buf = realloc(no->buf, no->cap);
    }
    if ((sz = recv(sock, no->buf+no->sz, no->cap-no->sz, 0)) <= 0)
        return -1;
    no->sz += sz;

    do {
        rerun = 0;
        switch (no->state) {
            case NOLP_EXPECT:
                if (no->cap < no->expect) {
                    if (!(no->buf = realloc(no->buf, no->expect)))
                        return -1;
                    no->cap = no->expect;

                }
                if (no->sz >= no->expect) {
                    /* all expected data received, call the
                     * next_cb() function */
                    if (no->next_cb(no, no->buf, no->expect) != 0)
                        return -1;

                    if (no->sz == no->expect) {
                        no->sz = 0;
                        no->cap = NOLP_DEFAULT_BUFSZ;
                        if (!(no->buf = realloc(no->buf, NOLP_DEFAULT_BUFSZ)))
                            return -1;
                    } else {
                        x = (no->buf+no->sz)-(no->buf+no->expect);
                        no->sz = x;
                        memmove(no->buf, (no->buf+no->expect), x);
                        rerun = 1;
                    }
                    no->state = NOLP_CMD;
                }
                break;

            default:
                while ((p = memchr(no->buf+last, '\n', no->sz-last))) {
                    if (no->state == NOLP_LINE) {
                        no->state = NOLP_CMD;
                        no->next_cb(no, no->buf, no->sz-last);
                    } else {
                        /* NOLP_CMD */
                        if (!(s = memchr(no->buf, ' ', p-no->buf)))
                            s = p;
                        *s = '\0';
                        for (x=0;; x++) {
                            if (!no->fn[x].name) {
                                /* command not found */
                                return -1;
                            }
                            if (strcmp(no->fn[x].name, no->buf) == 0) {
                                if (no->fn[x].cb(no, s+1, p-(s+1)) != 0)
                                    return -1;
                                break;
                            }
                        }
                    }
                    last = 0;
                    if (p+1 < no->buf+no->sz) {
                        x = (no->buf+no->sz)-(p+1);
                        /* more data after this command, we'll move it 
                         * to the front */
                        no->sz = x;
                        memmove(no->buf, p+1, x);
                    } else
                        no->sz = 0;

                    if (no->state == NOLP_EXPECT) {
                        if (no->sz)
                            rerun = 1;
                        break;
                    }
                }
                break;
        }
    } while (rerun);

    return 0;
}

void
nolp_free(nolp_t *no)
{
    free(no->buf);
}

/** 
 * expect 'size' bytes to be sent, subsequent calls to
 * nolp_recv() will not invoke any command functions 
 * until the whole buffer is received. once the buffer is 
 * received, the given complete_cb() will be invoked and
 * the state will be set back to NOLP_CMD.
 *
 * return 0 on success, -1 on error
 **/
int
nolp_expect(nolp_t *no, int size,
            int (*complete_cb)(void*, char *, int))
{
    no->next_cb = complete_cb;
    /*no->sz = 0;*/
    no->expect = size;
    no->state = NOLP_EXPECT;

    return 0;
}

