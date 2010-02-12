/*-
 * conn.h
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

#ifndef _NOL_MASTER_CONN__H_
#define _NOL_MASTER_CONN__H_

#include <ev.h>
#include <netinet/in.h>
#include <arpa/inet.h>

enum {
    NOL_LEVEL_NONE          = 0,
    NOL_LEVEL_READ          = 1,
    NOL_LEVEL_WRITE         = 2,
    NOL_LEVEL_MANAGER       = 1024,
    NOL_LEVEL_SIGNALS       = 2048,
    NOL_LEVEL_ADMIN         = 8192,
};

struct conn {
    int  sock;
    int  auth;
    int  authenticated;
    int  action;
    int  user_id;
    int  level;
    /* if this is connection to a slave, the slave
     * "id" will be set here */
    int   slave_n;
    struct sockaddr_in addr;
    ev_io fd_ev;
};

enum {
    NOL_AUTH_TYPE_CLIENT = 0,
    NOL_AUTH_TYPE_SLAVE,
    NOL_AUTH_TYPE_USER,

    NUM_AUTH_TYPES,
};

void nol_m_ev_conn_accept(EV_P_ ev_io *w, int revents);
void nol_m_conn_close(struct conn *conn);
int  nol_m_getline(int fd, char *buf, int max);

#endif
