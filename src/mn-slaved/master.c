/*-
 * master.c
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

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "slave.h"
#include "client.h"
#include "nolp.h"
#include "hook.h"

static int on_kill_all(nolp_t *no, char *buf, int size);
static int on_config(nolp_t *no, char *buf, int size);
static int on_config_recv(nolp_t *no, char *buf, int size);
static int on_client(nolp_t *no, char *buf, int size);
static int on_hook(nolp_t *no, char *buf, int size);
static int on_hook_recv(nolp_t *no, char *buf, int size);
void       msg_all_clients(int msg);

/**
 * nolp commands, master -> slave.
 *
 * note that ALL of these functions will be run in the MAIN 
 * thread
 **/
struct nolp_fn master_commands[] = {
    {"KILL-ALL", &on_kill_all},
    {"CONFIG", &on_config},
    {"CLIENT", &on_client},
    {"HOOK",   &on_hook},
    {0}
};

/**
 * Send a message to all client threads, which in turn will notify each
 * connected client
 **/
void
msg_all_clients(int msg)
{
    int            x;
    struct client *cl;

    pthread_mutex_lock(&srv.clients_lk);
    for (x=0; x<srv.num_clients; x++) {
        cl = srv.clients[x];
        cl->msg = msg;
        ev_async_send(cl->loop, &cl->async);
    }
    pthread_mutex_unlock(&srv.clients_lk);
}

/** 
 * KILL-ALL\n
 *
 * This command will force all client connections to be closed
 **/
static int
on_kill_all(nolp_t *no, char *buf, int size)
{
    int x;
#ifdef DEBUG
    syslog(LOG_DEBUG, "KILL-ALL received");
#endif
    msg_all_clients(NOL_CLIENT_MSG_KILL);
    return 0;
}

/** 
 * CONFIG <size>\n
 * we'll set the nolp to expect <size> bytes
 **/
static int
on_config(nolp_t *no, char *buf, int size)
{
    return nolp_expect(no, atoi(buf), &on_config_recv);
}

/* receive the configuration buffer, called
 * after on_config() */
static int
on_config_recv(nolp_t *no, char *buf, int size)
{
    /* if we have reconnect to the master, srv.config_buf will
     * already be allocated since earlier, and thus we must 
     * free it */
    if (srv.config_buf)
        free(srv.config_buf);
    if (!(srv.config_buf = malloc(size))) /* out of mem? */
        return -1;
    memcpy(srv.config_buf, buf, size);
    srv.config_sz = size;

    syslog(LOG_INFO, "read config from master");

    /* once we have a configuration ready, we can start
     * accepting clients */
    nol_s_set_ready();
    return 0;
}

/** 
 * syntax: CLIENT <ip-address> <username>\n
 *
 * create a client descriptor and add it to the pending list
 **/
static int
on_client(nolp_t *no, char *buf, int size)
{
    char           out[TOKEN_SIZE+5];
    struct client *cl;
    char          *s;
    int            ok;

    if (!srv.ready) {
        send(no->fd, "400\n", 4, 0);
        return 0;
    }

    buf[size] = 0;
    ok = 0;
    if (!(s = memchr(buf, ' ', size)))
        return -1;
    if (!(cl = nol_s_client_create(buf, s+1)))
        return -1;

    pthread_mutex_lock(&srv.pending_lk);
    if (srv.num_pending < MAX_NUM_PENDING) {
        ok = 1;
        /* add this client to the pending list */
        if (!(srv.pending = realloc(srv.pending,
                        (srv.num_pending+1)*sizeof(struct client*)))) {
            syslog(LOG_ERR, "out of mem");
            abort();
        }

        srv.pending[srv.num_pending] = cl;
        srv.num_pending ++;
    }
    pthread_mutex_unlock(&srv.pending_lk);

    if (!ok) {
        send(no->fd, "400\n", 4, 0);
        nol_s_client_free(cl);
    } else {
        ok = sprintf(out, "100 %40s\n", cl->token);
        send(no->fd, out, ok, 0);
    }

    return 0;
}

static char hook_name[32];
/** 
 * HOOK <name> <length>\n
 **/
static int
on_hook(nolp_t *no, char *buf, int size)
{
    int expect;
    sscanf(buf, "%31s %d", hook_name, &expect);
    return nolp_expect(no, expect, &on_hook_recv);
}

/* receive the script for a hook, the name of the 
 * hook have been set by on_hook() in hook_name */
static int
on_hook_recv(nolp_t *no, char *buf, int size)
{
    nol_s_hook_assign(hook_name, buf, size);

    return 0;
}

