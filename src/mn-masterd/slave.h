/*-
 * slave.h
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

#ifndef _SLAVE__H_
#define _SLAVE__H_

#include "lmc.h"
#include "conn.h"
#include "client.h"

enum {
    SL_STATE_NONE,
    SL_STATE_RECV_TOKEN,
    SL_STATE_RECV_STATUS,
};

typedef struct slave {
    char     *name;
    char     *password;
    uint32_t  flags;
} slave_t;

typedef struct slave_conn {
    char          *name;
    int            name_len;
    int            id;
    struct conn   *conn;
    client_conn_t *clients;
    int            num_clients;
    /* client conn will be set if we are waiting for
     * a TOKEN for the given client */
    struct conn   *client_conn;

    struct {
        struct {
            char *buf;
            int   sz;
        } clients;
    } xml;

    /* set by the INFO command */
    char          listen_addr[16];
    uint16_t      listen_port;
    int            ready;
} slave_conn_t;

void nol_m_auth_cleanup_slaves(void);
extern struct lmc_class nol_slave_class;
slave_conn_t *nol_m_create_slave_conn(const char *user);

#endif
