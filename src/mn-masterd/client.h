/*-
 * client.h
 * This file is part of Methanol
 * http://metha-sys.org/
 * http://bithack.se/projects/methabot/
 *
 * Copyright (c) 2009, Emil Romanus <sdac@bithack.se>
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

#ifndef _NOL_CLIENT__H_
#define _NOL_CLIENT__H_

#include "lmc.h"
#include "conn.h"

/* describe a client object as declared in mn-masterd.conf, 
 * created and filled by an lmc parser
 * */
typedef struct client {
    char     *name;
    char     *password;
    char     *slave;
    uint32_t  flags;
} client_t;

typedef struct client_conn {
    char         token[40];
    char        *user;
    char        *addr;
    unsigned int status;
    unsigned int state;
    unsigned int session_id;
} client_conn_t;

extern struct lmc_class nol_client_class;

#endif

