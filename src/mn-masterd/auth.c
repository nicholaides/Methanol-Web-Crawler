/*-
 * conn.c
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

#include "conn.h"
#include "master.h"
#include "nolp.h"

#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int nol_m_create_slave_list_xml(void);

extern struct nolp_fn slave_commands[];
extern struct nolp_fn user_commands[];

const char *auth_types[] = {
    "client",
    "slave",
    "user",
};

static void user_read(EV_P_ ev_io *w, int revents);
static void slave_read(EV_P_ ev_io *w, int revents);
static int nol_m_token_reply(nolp_t *no, char *buf, int size);
/* user.c */
void nol_m_user_read(EV_P_ ev_io *w, int revents);

static void conn_read(EV_P_ ev_io *w, int revents);
static int upgrade_conn(struct conn *conn, const char *user);
static int check_user_login(const char *user, const char *pwd);
static int check_slave_login(const char *user, const char *pwd);
static int check_client_login(const char *user, const char *pwd);
static int send_config(int sock);
static int send_hooks(int sock);

/** 
 * Accept a connection and set up a conn struct
 **/
void
nol_m_ev_conn_accept(EV_P_ ev_io *w, int revents)
{
    int sock;
    socklen_t sin_sz;

    struct conn *c = malloc(sizeof(struct conn));

    memset(&c->addr, 0, sizeof(struct sockaddr_in));
    sin_sz = sizeof(struct sockaddr_in);

    if (!c || !(srv.pool = realloc(srv.pool, sizeof(struct conn)*(srv.num_conns+1)))) {
        syslog(LOG_ERR, "fatal: out of mem");
        abort();
    }

    srv.pool[srv.num_conns] = c;
    srv.num_conns ++;

    if ((sock = accept(w->fd, (struct sockaddr *)&c->addr, &sin_sz)) == -1) {
        syslog(LOG_ERR, "fatal: accept() failed: %s", strerror(errno));
        abort();
    }

    syslog(LOG_INFO, "#%d accepted connection from %s",
            sock,
            inet_ntoa(c->addr.sin_addr));

    c->sock = sock;
    c->auth = 0;
    c->authenticated = 0;

    /* add an event listenr for this sock */
    ev_io_init(&c->fd_ev, &conn_read, sock, EV_READ);
    ev_io_start(EV_A_ &c->fd_ev);

    c->fd_ev.data = c;
}

int
nol_m_getline(int fd, char *buf, int max)
{
    int sz;
    char *nl;

    if ((sz = recv(fd, buf, max, MSG_PEEK)) <= 0)
        return -1;
    if (!(nl = memchr(buf, '\n', sz)))
        return 0;
    if ((sz = read(fd, buf, nl-buf+1)) == -1)
        return -1;

    return sz;
}

/** 
 * Called when data is available on a socket 
 * connected to a slave.
 **/
static void
slave_read(EV_P_ ev_io *w, int revents)
{
    nolp_t *no = (nolp_t *)w->data;

    if (nolp_recv(no) != 0) {
        ev_io_stop(EV_A_ w);
        nol_m_conn_close(no->private);
    }
}

/** 
 * Called when data is available on a socket 
 * connected to a user. Interpret commands 
 * such as status and search queries.
 **/
static void
user_read(EV_P_ ev_io *w, int revents)
{
    nolp_t *no = (nolp_t *)w->data;

    if (nolp_recv(no) != 0) {
        ev_io_stop(EV_A_ w);
        nol_m_conn_close(no->private);
    }
}


static void
conn_read(EV_P_ ev_io *w, int revents)
{
    int sock = w->fd;
    char buf[1024];
    int sz;
    char *user;
    char *pwd;
    char *type;
    char *e;
    int x;

    struct conn* conn = w->data;
    struct slave *sl;

    if ((sz = nol_m_getline(sock, buf, 1023)) <= 0)
        goto close;

    buf[sz] = '\0';

    if (conn->authenticated) {
        /* user & slave connections will change event callback
         * to the ones found in slave.c and user.c */
        goto close;
    } else {
        if (memcmp(buf, "AUTH", 4) != 0)
            goto denied;
        type=buf+5;
        e=buf+sz;
        if (!(user = memchr(type, ' ', e-type)))
            goto close;
        *user = '\0';
        user ++;
        if (!(pwd = memchr(user, ' ', e-user)))
            goto close;
        *pwd = '\0';
        pwd++;
        char *p;
        for (p=e; isspace(*p) || !*p; p--)
            *p = '\0';
        
        /* verify the auth type */
        int auth_found = 0;
        for (x=0; x<NUM_AUTH_TYPES; x++) {
            if (strcmp(type, auth_types[x]) == 0) {
                conn->auth = x;
                auth_found = 1;
                break;
            }
        }
        
        if (!auth_found)
            goto denied;

        if (conn->auth == NOL_AUTH_TYPE_USER) {
            if ((conn->user_id = check_user_login(user, pwd)) == -1)
                goto denied;
        } else if (conn->auth == NOL_AUTH_TYPE_SLAVE) {
            if (check_slave_login(user, pwd) != 0)
                goto denied;
        } else if (conn->auth == NOL_AUTH_TYPE_CLIENT) {
            if (check_client_login(user, pwd) != 0)
                goto denied;
        } else
            goto denied;


        if (conn->auth != NOL_AUTH_TYPE_USER)
            syslog(LOG_INFO, "AUTH type=%s,user=%s OK from #%d", type, user, sock);
        conn->authenticated = 1;

        send(sock, "100 OK\n", 7, 0);
        if (upgrade_conn(conn, user) != 0)
            goto close;
    }
    return;


denied:
    send(sock, "200 Denied\n", 11, MSG_NOSIGNAL);
close:
    ev_io_stop(EV_A_ &conn->fd_ev);
    nol_m_conn_close(conn);
    return;

invalid:
    syslog(LOG_WARNING, "invalid data from #%d, closing connection", sock);
    send(sock, "201 Bad Request\n", 16, MSG_NOSIGNAL);
    close(sock);
}

/**
 * verify the login
 *
 * this function does NOT check for buffer overflows using 
 * long user or pwd strings, and should only be used 
 * where those two have already been cut
 *
 * return the user id on success, or -1 if the login 
 * failed.
 **/
static int
check_user_login(const char *user, const char *pwd)
{
    char buf[64*3];
    int len;
    MYSQL_RES *r;
    MYSQL_ROW row;
    int ret = -1;

    strrmsq(user);
    strrmsq(pwd);

    len = sprintf(buf, "SELECT id FROM nol_user WHERE user = '%s' AND pass = MD5('%s');",
            user, pwd);
    if (mysql_real_query(srv.mysql, buf, len) == 0) {
        if ((r = mysql_store_result(srv.mysql))) {
            if ((row = mysql_fetch_row(r)))
               ret = atoi(row[0]);
            mysql_free_result(r);
        }
    } else {
        syslog(LOG_ERR, "user auth error: %s", mysql_error(srv.mysql));
    }

    return ret;
}

/**
 * verify the slave login
 *
 * return 0 if the login is valid
 **/
static int
check_slave_login(const char *user, const char *pwd)
{
    int x;
    for (x=0; x<srv.auth.num_slaves; x++) {
        if (strcmp(srv.auth.slaves[x]->name, user) == 0) {
            /* found the username, if the password does not 
             * match then we return -1 */
            if (strcmp(srv.auth.slaves[x]->password, pwd) == 0)
                return 0;
            else
                return -1;
        }
    }

    /* username not found */
    return -1;
}
/**
 * verify the client login
 *
 * return 0 if valid
 **/
static int
check_client_login(const char *user, const char *pwd)
{
    int x;
    for (x=0; x<srv.auth.num_clients; x++) {
        if (strcmp(srv.auth.clients[x]->name, user) == 0) {
            /* found the username, if the password does not 
             * match then we return -1 */
            if (strcmp(srv.auth.clients[x]->password, pwd) == 0)
                return 0;
            else
                return -1;
        }
    }

    /* username not found */
    return -1;
}


/** 
 * send the active configuration to the given sock,
 * used to send the configuration to connected 
 * slaves.
 *
 * return 0 on success
 **/
static int
send_config(int sock)
{
    int  sz;
    char buf[64];

    sz = sprintf(buf, "CONFIG %d\n", srv.config_sz);
    if (send(sock, buf, sz, 0) > 0 && 
        send(sock, srv.config_buf, srv.config_sz, 0) > 0)
        return 0;

    return -1;
}

/** 
 * send all hook scripts to the given sock, used
 * to send them to newly connected slaves
 *
 * return 0 on success
 **/
static int
send_hooks(int sock)
{
    char buf[96];
    int  x;
    int  sz;
    static const char *s[] = {
        "session-complete",
        "cleanup",
    };
    for (x=0; x<NUM_HOOKS; x++) {
        if (srv.hooks[x].sz) {
            sz = sprintf(buf, "HOOK %.32s %d\n",
                    s[x], (int)srv.hooks[x].sz
                    );
            if (send(sock, buf, sz, 0) <= 0)
                return -1;
            if (send(sock, srv.hooks[x].buf, srv.hooks[x].sz, 0) <= 0)
                return -1;
        }
    }

    return 0;
}

/** 
 * remove single-quotes
 **/
void
strrmsq(char *s)
{
    while (*s) {
        if (*s == '\'')
            *s = '_';
        s++;
    }
}

/** 
 * upgrade the connection depending on what value
 * conn->auth is set to. Change the event loop
 * callback function and create a slave structure
 * if the conn is a slave
 *
 * return 0 unless an error occurs
 **/
static int
upgrade_conn(struct conn *conn, const char *user)
{
    int           sz;
    nolp_t       *no;
    char          buf[96];
    slave_conn_t *sl;
    int  sock = conn->sock;
    MYSQL_RES *r;
    MYSQL_ROW row;

    switch (conn->auth) {
        case NOL_AUTH_TYPE_CLIENT:
            /* give this client to a slave */
            if (!srv.num_slaves) {
                /* add to pending list */
                return -1;
            } else {
                unsigned min = (unsigned)-1;
                unsigned min_o;
                unsigned x;
                /* find the slave with the least num clients connected, 
                 * and connect this client with that slave, also verify 
                 * that the slave isn't currently generating a token for 
                 * another client */
                for (x=0; x<srv.num_slaves; x++) {
                    slave_conn_t *sl = &srv.slaves[x];
                    if (sl->ready && sl->num_clients < min
                            && !sl->client_conn) {
                        min = sl->num_clients;
                        min_o = x;
                    }
                }
                /* the slave with the least amount of clients should now be at
                 * srv.slaves[min_o] */
                if (min == (unsigned)-1)
                    /* no slave found, or they were all busy */
                    return -1;

                sz = sprintf(buf, "CLIENT %s %.64s\n", inet_ntoa(conn->addr.sin_addr), user);
                srv.slaves[min_o].client_conn = conn; 
                nolp_expect_line((nolp_t *)(srv.slaves[min_o].conn->fd_ev.data),
                        &nol_m_token_reply);
                send(srv.slaves[min_o].conn->sock, buf, sz, 0);
            }
            break;

        case NOL_AUTH_TYPE_SLAVE:
            if (!(sl = nol_m_create_slave_conn(user)))
                return -1;
            if (!(no = nolp_create(slave_commands, sock)))
                return -1;

            conn->slave_n = srv.num_slaves-1;
            sl->conn      = conn;
            no->private = conn;
            conn->fd_ev.data = no;

            send_config(sock);
            send_hooks(sock);
            nol_m_create_slave_list_xml();

            /* from now on, the data received from this
             * socket will be parsed by the nolp
             * struct, in slave-conn.c */
            ev_set_cb(&conn->fd_ev, &slave_read);
            break;

        case NOL_AUTH_TYPE_USER:
            /* find out and store the permissions for this user */
            sz = sprintf(buf, "SELECT level FROM `nol_user` WHERE id = %d", conn->user_id);
            if (mysql_real_query(srv.mysql, buf, sz) != 0)
                return -1;
            if (!(r = mysql_store_result(srv.mysql)))
                return -1;
            if (!(row = mysql_fetch_row(r))) {
                mysql_free_result(r);
                return -1;
            }
            conn->level = atoi(row[0]);
            syslog(LOG_DEBUG, "#%d set permission level to %d", sock, conn->level);
            mysql_free_result(r);

            if (!(no = nolp_create(user_commands, sock)))
                return -1;

            ev_set_cb(&conn->fd_ev, &user_read);
            no->private = conn;
            conn->fd_ev.data = no;
            break;

        default:
            return 1;
    }

    return 0;
}

void
nol_m_conn_close(struct conn *conn)
{
    int x;

    if (conn->authenticated) {
        if (conn->auth == NOL_AUTH_TYPE_CLIENT) {
            for (x=0; x<srv.num_slaves; x++) {
                if (srv.slaves[x].client_conn == conn) {
                    srv.slaves[x].client_conn = 0;
                    break;
                }
            }
        } else if (conn->auth == NOL_AUTH_TYPE_SLAVE) {
            syslog(LOG_INFO, "slave %s-%d disconnected",
                    srv.slaves[conn->slave_n].name,
                    srv.slaves[conn->slave_n].id);
            /* move the top-most slave to this slave's
             * position in the list, if this slave isn't
             * already the top-most one */
            if (conn->slave_n != srv.num_slaves-1) {
                srv.slaves[conn->slave_n] = srv.slaves[srv.num_slaves];
                srv.slaves[conn->slave_n].conn->slave_n = conn->slave_n;
            }
            if (srv.num_slaves == 1) {
                free(srv.slaves);
                srv.slaves = 0;
            } else if (!(srv.slaves = realloc(srv.slaves, (srv.num_slaves-1)*sizeof(struct slave)))) {
                syslog(LOG_ERR, "out of mem");
                abort();
            }
            srv.num_slaves --;
            /* refresh the XML list of slaves */
            nol_m_create_slave_list_xml();
        }
    }

    close(conn->sock);
    free(conn);

    for (x=0; x<srv.num_conns; x++) {
        if (srv.pool[x] == conn) {
            srv.pool[x] = srv.pool[srv.num_conns-1];
            break;
        }
    }

    srv.num_conns --;
}


/** 
 * return 0 if the token was read and sent to the 
 * corresponding client successfully
 **/
static int
nol_m_token_reply(nolp_t *no, char *buf, int size)
{
    int           sz;
    char          out[70];
    struct conn  *client;
    struct conn  *conn = (struct conn*)no->private;
    slave_conn_t *sl = &srv.slaves[conn->slave_n];
    if (!(client = sl->client_conn))
        /* client must have disconnected before 
         * we replied */
        return 0;

    if (atoi(buf) == 100) {
        sz = sprintf(out, "TOKEN %.40s-%.15s:%hd\n", buf+4,
                sl->listen_addr,
                sl->listen_port);
        send(client->sock, out, sz, 0);
    }

    /* stop and disconnect the client connection */
    ev_io_stop(EV_DEFAULT, &client->fd_ev);
    nol_m_conn_close(client);

    return 0;
}

