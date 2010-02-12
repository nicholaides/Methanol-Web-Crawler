/*-
 * master.h
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

#ifndef _MASTER__H_
#define _MASTER__H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <mysql.h>

#include "methanol.h"
#include "slave.h"
#include "client.h"
#include "conn.h"

enum {
    HOOK_SESSION_COMPLETE,
    HOOK_CLEANUP,
    NUM_HOOKS,
};

struct crawler;
struct filetype;

struct opt_val_list {
    char *listen;
    char *config_file;
    char *user;
    char *group;
    char *mysql_host;
    unsigned int mysql_port;
    char *mysql_sock;
    char *mysql_user;
    char *mysql_pass;
    char *mysql_db;
    char *hooks[NUM_HOOKS];
};

/* run-time generated information */
struct master {
    MYSQL             *mysql;
    struct sockaddr_in addr;
    int                listen_sock;
    char              *config_buf;
    int                config_sz;

    struct conn      **pool;
    unsigned           num_conns;
    slave_conn_t      *slaves;
    unsigned           num_slaves;
    struct filetype  **filetypes;
    unsigned           num_filetypes;
    struct crawler   **crawlers;
    unsigned           num_crawlers;

    /** 
     * used for authenticating slaves and clients,
     * filled by an lmc parser using classes 
     * defined in client-class.c and slave-class.c
     **/
    struct {
        slave_t   **slaves;
        unsigned    num_slaves;
        client_t  **clients;
        unsigned    num_clients;
    } auth;

    struct {
        char     *buf;
        size_t    sz;
    } hooks[NUM_HOOKS];

    struct {
        struct {
            char *buf;
            int   sz;
        } slave_list;
    } xml;

    /* set in main(), only used for calculating the uptime of the master */
    time_t start_time;
};

extern struct master srv;
extern struct opt_val_list opt_vals;

void *nol_m_conn_main(void *in);
void strrmsq(char *s);

#endif
