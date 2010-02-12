/*-
 * slave-conn.c
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <ev.h>
#include "master.h"
#include "slave.h"
#include "nolp.h"

int nol_m_create_slave_list_xml();

static int sl_status_command(nolp_t *no, char *buf, int size);
static int sl_info_command(nolp_t *no, char *buf, int size);
static int sl_session_complete_command(nolp_t *no, char *buf, int size);
static int sl_status_parse(nolp_t *no, char *buf, int size);
static int read_token(EV_P_ struct conn *conn);
static int call_session_complete_hook(long sess_id);

struct nolp_fn slave_commands[] = {
    {"STATUS", &sl_status_command},
    {"INFO", &sl_info_command},
    {0}
};

/** 
 * Create a new slave descriptor, create a name for the 
 * slave by querying the database. Also add the slave to 
 * the global slave list.
 **/
slave_conn_t *
nol_m_create_slave_conn(const char *user)
{
    slave_conn_t *r;
    static char  q[128];
    int sz;
    int x;

    /* Add this slave to the list of slaves */
    if (!(srv.slaves = realloc(srv.slaves,
                    (srv.num_slaves+1)*sizeof(slave_conn_t)))) {
        syslog(LOG_ERR, "out of mem");
        abort();
    }
    r = &srv.slaves[srv.num_slaves];
    srv.num_slaves ++;

    sz = strlen(user);
    if (sz > 64) sz = 64;
    r->name = malloc(sz+1);
    *(r->name+sz) = '\0';
    memcpy(r->name, user, sz);
    /* convert anything but 09A-Za-z_ to _ to prevent
     * sql injections and user confusion */
    for (x=0; x<sz; x++)
        if (!isalnum(*(r->name+x)))
            *(r->name+x) = '_';
    r->name_len = sz;

    sz = sprintf(q, "INSERT INTO nol_slave (user) VALUES ('%s');", r->name);
    if (mysql_real_query(srv.mysql, q, sz) != 0) {
        free(r->name);
        srv.num_slaves --;
        return 0;
    }
    r->id = (int)mysql_insert_id(srv.mysql);

    r->xml.clients.buf = 0;
    r->xml.clients.sz = 0;
    r->num_clients = 0;
    r->client_conn = 0;
    r->clients = 0;

    syslog(LOG_INFO, "registered slave: %s-%d", r->name, r->id);
    
    return r;
}

int
nol_m_create_slave_list_xml()
{
    int x;

    /* recreate the XML for the slave list */
    srv.xml.slave_list.buf =
        realloc(
            srv.xml.slave_list.buf, 
            srv.num_slaves*
              (64+20+20+15+6+ /* name length + id length + num clients + addr length + port length */
               sizeof("<slave id=\"\"><user></user><num-clients></num-clients><address></address></slave>")-1
              )+
              sizeof("<slave-list></slave-list>")
            );
    char *p = srv.xml.slave_list.buf;
    p+=sprintf(p, "<slave-list>");
    for (x=0; x<srv.num_slaves; x++)
        p+=sprintf(p,
                "<slave id=\"%d\">"
                  "<user>%.64s</user>"
                  "<num-clients>%d</num-clients>"
                  "<address>%.15s:%hd</address>"
                "</slave>",
                srv.slaves[x].id,
                srv.slaves[x].name,
                srv.slaves[x].num_clients,
                (srv.slaves[x].ready ? srv.slaves[x].listen_addr : "0"),
                (srv.slaves[x].ready ? srv.slaves[x].listen_port : 0)
                );
    p+=sprintf(p, "</slave-list>");
    srv.xml.slave_list.sz = p-srv.xml.slave_list.buf;
    srv.xml.slave_list.buf =
        realloc(
            srv.xml.slave_list.buf, 
            srv.xml.slave_list.sz);

    return 0;
}


/**
 * prepare for receiving the status buffer
 *
 * STATUS <size-in-bytes>\n
 * <hash>,<status>\n
 * ...
 **/
static int
sl_status_command(nolp_t *no, char *buf, int size)
{
    if (atoi(buf) == 0)
        return sl_status_parse(no, buf, 0);
    return nolp_expect(no, atoi(buf), &sl_status_parse);
}

/** 
 * receive information about listening address and port,
 * once this command has been received, the slave will
 * be marked as 'ready' and clients will be distributed
 * to it.
 *
 * INFO <listening-addr> <port>\n
 **/
static int
sl_info_command(nolp_t *no, char *buf, int size)
{
    struct conn *conn = no->private;
    slave_conn_t *sl  = &srv.slaves[conn->slave_n];

    if (sscanf(buf, "%15s %hd", sl->listen_addr, &sl->listen_port) != 2)
        return -1;
#ifdef DEBUG
    syslog(LOG_DEBUG, "slave %d is ready, listening on %s:%hd",
            sl->id, sl->listen_addr, sl->listen_port);
#endif 

    sl->ready = 1;
    nol_m_create_slave_list_xml();
    return 0;
}

/**
 * parse the status buffer, generate both XML and internal
 * representations
 *
 * xml syntax:
 * <client-list for="slave_name">
 *   <client id="sha1-hash">
 *     <user>username</user>
 *     <status>status</status>
 *     <address>address</address>
 *   </client>
 *   ...
 * </client-list>
 **/
static int
sl_status_parse(nolp_t *no, char *buf, int size)
{
    int x;
    int xml_sz = 0;
    char *p, *rp, *re;
    struct conn *conn = no->private;
    slave_conn_t *sl = &srv.slaves[conn->slave_n];
    client_conn_t *curr;
    char user[65];
    char address[16];
    int status;

    for (x=0; x<sl->num_clients; x++) {
        free(sl->clients[x].user);
        free(sl->clients[x].addr);
    }
    sl->num_clients = 0;
    rp = buf;
    re = buf+size;
    while (rp < re-1) {
        if (!(sl->clients = realloc(sl->clients, sizeof(client_conn_t)*(sl->num_clients+1))))
            return -1;
        curr = &sl->clients[sl->num_clients];
        sscanf(rp, "%40s%15s%64s%u%u",
                &curr->token, &address, &user,
                &curr->status, &curr->session_id);
        curr->user = strdup(user);
        curr->addr = strdup(address);
        sl->num_clients ++;
        xml_sz += strlen(user)+strlen(address)+40+1;
        if (!(rp = memchr(rp+1, '\n', re-rp)))
            break;
    }

    char  slave_str[80];
    int   slave_str_len
        = sprintf(slave_str, "%.64s-%d", sl->name, sl->id);
    /* generate XML information */
    if (!(p = (sl->xml.clients.buf = realloc(sl->xml.clients.buf,
                    xml_sz+
                    slave_str_len+
                    sl->num_clients*(
                        sizeof("<client id=\"\"><user></user><slave></slave><status></status><address></address></client>")-1
                        +slave_str_len
                        +15+40+64+15
                        )+
                    sizeof("<client-list for=\"\"></client-list>")))))
        return -1;
    p += sprintf(p, "<client-list for=\"%s\">", slave_str);
    for (x=0; x<sl->num_clients; x++) {
        curr = &sl->clients[x];
        p+=sprintf(p, 
                "<client id=\"%.40s\">"
                  "<user>%.64s</user>"
                  "<slave>%s</slave>"
                  "<status>%d</status>"
                  "<address>%.15s</address>"
                "</client>",
                curr->token,
                curr->user,
                slave_str, 
                (curr->status & 1),
                curr->addr);
    }
    p += sprintf(p, "</client-list>");

    x = p-sl->xml.clients.buf;
    sl->xml.clients.buf = realloc(sl->xml.clients.buf, x);
    sl->xml.clients.sz = x;

    nol_m_create_slave_list_xml();

    return 0;
}

#if 0
/* check if a session is marked as wait-postprocess */
static void
check_sessions()
{
    MYSQL_RES *r;
    MYSQL_ROW row;
    long sess_id;
    char q[128];

#ifdef DEBUG
    syslog(LOG_DEBUG, "checking sessions");
#endif

    mysql_query(srv.mysql,
            "SELECT id FROM nol_session "
            "WHERE state='wait-hook' ORDER BY date DESC");
    if (!(r = mysql_store_result(srv.mysql)))
        return;

    while ((row = mysql_fetch_row(r))) {
        sess_id = atol(row[0]);
#ifdef DEBUG 
        syslog(LOG_DEBUG, "calling session-complete hook for #%ld", sess_id);
#endif
        sprintf(q, "UPDATE `nol_session` SET state='hook' WHERE id=%d", sess_id);
        mysql_query(srv.mysql, q);
        call_session_complete_hook(sess_id);
        sprintf(q, "UPDATE `nol_session` SET state='done' WHERE id=%d", sess_id);
        mysql_query(srv.mysql, q);
    }
    mysql_free_result(r);
}
#endif

