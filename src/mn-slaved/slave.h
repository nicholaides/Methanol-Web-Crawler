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

#ifndef _M_SLAVE__H_
#define _M_SLAVE__H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mysql/mysql.h>
#include <ev.h>

#include "nolp.h"

#define MAX_NUM_PENDING  128

struct opt_vals {
    char *listen;
    char *master_host;
    unsigned int master_port;
    char *master_user;
    char *master_password;

    char *user;
    char *group;
    char *mysql_host;
    unsigned int mysql_port;
    char *mysql_sock;
    char *mysql_user;
    char *mysql_pass;
    char *mysql_db;
    char *exec_dir;
} opt_vals;

/* runtime information about this slave */
struct slave {
    MYSQL              *mysql;
    nolp_t             *m_nolp;

    int                 listen_sock;
    struct sockaddr_in  addr;
    struct sockaddr_in  master;

    char          *config_buf;
    unsigned int   config_sz;

    int   ready;
    int   master_sock;

    ev_io     master_io;
    /* timer for reconnecting to the master if the connection is lost */
    ev_timer  master_timer;
    ev_async  client_status;

    struct client  **pending;
    int              num_pending;
    pthread_mutex_t  pending_lk;
    struct client **clients;
    int             num_clients;
    pthread_mutex_t  clients_lk;
};

extern struct slave srv;
extern struct opt_vals opt_vals;

int sock_getline(int fd, char *buf, int max);
MYSQL *nol_s_dup_mysql_conn(void);
int nol_s_set_ready(void);

#endif
