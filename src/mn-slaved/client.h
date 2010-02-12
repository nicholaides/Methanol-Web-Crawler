/*-
 * client.h
 * This file is part of Methanol
 *
 * Copyright (c) 2009, Emil Romanus <sdac@bithack.se>
 * http://metha-sys.org/
 * http://bithack.se/projects/methabot/
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
 */

#ifndef _CLIENT__H_
#define _CLIENT__H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ev.h>

#define TOKEN_SIZE 40

enum {
    NOL_CLIENT_MSG_KILL,
    NOL_CLIENT_MSG_STOP,
    NOL_CLIENT_MSG_PAUSE,
    NOL_CLIENT_MSG_CONTINUE,
};

struct client {
    long               id;
    long               session_id;
    long               target_id;
    char               token[TOKEN_SIZE+1];
    int                running;
    int                msg; /* set externally before the 'async' ev is invoked */
    char              *user;
    struct in_addr     addr;
    struct ev_loop    *loop;
    ev_async           async;
    ev_timer           timer;
    void              *no;
    void              *mysql;
    char               filetype_name[64];
};

void *nol_s_client_init(void *in);
struct client *nol_s_client_create(const char *addr, const char *user);
void nol_s_client_free(struct client *cl);

#endif
