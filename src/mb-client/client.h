/*-
 * client.h
 * This file is part of mb-client 
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

#ifndef _MBC_CLIENT__H_
#define _MBC_CLIENT__H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ev.h>
#include "../libmetha/metha.h"
#include "nolp.h"

extern int verbose;

#define print_info(X, ...) fprintf(stdout, "[I] " X "\n", __VA_ARGS__)
#define print_infov(X, ...) {if(verbose)fprintf(stdout, "[I] " X "\n", __VA_ARGS__);}
#define print_error(X, ...) fprintf(stderr, "[E] " X "\n", __VA_ARGS__)
#define print_warning(X, ...) fprintf(stderr, "[W] " X "\n", __VA_ARGS__)
#ifdef DEBUG
#define print_debug(X, ...) fprintf(stderr, "* " X "\n", __VA_ARGS__)
#endif

enum {
    MBC_STATE_DISCONNECTED,
    MBC_STATE_WAIT_LOGIN,
    MBC_STATE_WAIT_TOKEN,
    MBC_STATE_WAIT_CONFIG,
    MBC_STATE_WAIT_START,
    MBC_STATE_PAUSED,
    MBC_STATE_STOPPED,
    MBC_STATE_RUNNING,
};

struct mbc {
    metha_t            *m;
    nolp_t             *no; /* used for receiving commands from the slave */
    struct ev_loop     *loop;
    ev_io              sock_ev;
    ev_timer           timer_ev;
    ev_async           idle_ev;
    int                state;
    int                sock; /* element used for both the master and the slave socket */
    struct sockaddr_in addr;
    struct sockaddr_in master;
    struct sockaddr_in slave;
}; 

extern struct mbc mbc;

int mbc_end_session(void);

#endif
