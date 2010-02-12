/*-
 * user.c
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

#include "master.h"
#include "slave.h"
#include "nolp.h"

#include <ev.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int user_list_clients_command(nolp_t *no, char *buf, int size);
static int user_list_slaves_command(nolp_t *no, char *buf, int size);
static int user_list_users_command(nolp_t *no, char *buf, int size);
static int user_slave_info_command(nolp_t *no, char *buf, int size);
static int user_client_info_command(nolp_t *no, char *buf, int size);
static int user_show_config_command(nolp_t *no, char *buf, int size);
static int user_log_command(nolp_t *no, char *buf, int size);
static int user_add_command(nolp_t *no, char *buf, int size);
static int user_useradd_command(nolp_t *no, char *buf, int size);
static int user_userdel_command(nolp_t *no, char *buf, int size);
static int user_passwd_command(nolp_t *no, char *buf, int size);
static int user_passwd_id_command(nolp_t *no, char *buf, int size);
static int user_session_info_command(nolp_t *no, char *buf, int size);
static int user_session_report_command(nolp_t *no, char *buf, int size);
static int user_list_sessions_command(nolp_t *no, char *buf, int size);
static int user_list_input_command(nolp_t *no, char *buf, int size);
static int user_kill_all_command(nolp_t *no, char *buf, int size);
static int user_system_info_command(nolp_t *no, char *buf, int size);
static int user_hello_command(nolp_t *no, char *buf, int size);

struct nolp_fn user_commands[] = {
    {"LIST-SLAVES", &user_list_slaves_command},
    {"LIST-CLIENTS", user_list_clients_command},
    {"LIST-USERS", user_list_users_command},
    {"SLAVE-INFO", user_slave_info_command},
    {"CLIENT-INFO", user_client_info_command},
    {"SHOW-CONFIG", user_show_config_command},
    {"LOG", user_log_command},
    {"ADD", user_add_command},
    {"USERADD", user_useradd_command},
    {"USERDEL", user_userdel_command},
    {"PASSWD", user_passwd_command},
    {"PASSWD-ID", user_passwd_id_command},
    {"SESSION-INFO", user_session_info_command},
    {"SESSION-REPORT", user_session_report_command},
    {"LIST-SESSIONS", user_list_sessions_command},
    {"LIST-INPUT", user_list_input_command},
    {"KILL-ALL", user_kill_all_command},
    {"SYSTEM-INFO", user_system_info_command},
    {"HELLO", user_hello_command},
    {0},
};

#define MSG100 "100 OK\n"
#define MSG200 "200 Denied\n"
#define MSG201 "201 Bad Request\n"
#define MSG202 "202 Login type unavailable\n"
#define MSG203 "203 Not found\n"
#define MSG300 "300 Internal Error\n"

/** 
 * The LIST-CLIENTS command requests a list of clients
 * connected to the given slave. Syntax:
 * LIST-CLIENTS <slave>\n
 * 'slave' should be the iid of a slave
 **/
static int
user_list_clients_command(nolp_t *no, char *buf, int size)
{
    int  x;
    int  id;
    char reply[32];
    struct conn *conn = (struct conn*)no->private;

    id = atoi(buf);

    for (x=0; ; x++) {
        if (x == srv.num_slaves) {
            send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
            break;
        }
        slave_conn_t *sl = &srv.slaves[x];
        if (sl->id == id) {
            int len = sprintf(reply, "100 %d\n", sl->xml.clients.sz);
            send(conn->sock, reply, len, 0);
            send(conn->sock, sl->xml.clients.buf, sl->xml.clients.sz, 0);
            break;
        }
    }

    return 0;
}

/** 
 * LIST-SLAVES returns a list of all slaves in XML format, 
 * including their client count. Syntax:
 * LIST-SLAVES 0\n
 * currently, the second argument is unused and should be 
 * set to 0.
 **/
static int
user_list_slaves_command(nolp_t *no, char *buf, int size)
{
    char reply[32];
    struct conn *conn = (struct conn*)no->private;
    int len = sprintf(reply, "100 %d\n", srv.xml.slave_list.sz);
    send(conn->sock, reply, len, 0);
    send(conn->sock, srv.xml.slave_list.buf, srv.xml.slave_list.sz, 0);

    return 0;
}

/** 
 * The SLAVE-INFO outputs information about a slave, such
 * as address and num connected clients, in XML format.
 * Syntax:
 * SLAVE-INFO <slave>\n
 * 'slave' should be the NAME of a slave
 **/
static int
user_slave_info_command(nolp_t *no, char *buf, int size)
{
    int x;
    int id;
    char tmp[32];
    char reply[
        sizeof("<slave-info for=\"\"><address></address></slave-info>")-1
        +64+20+16+6];
    struct conn  *conn = (struct conn*)no->private;
    slave_conn_t *sl = 0;

    id = atoi(buf);
    for (x=0; x<srv.num_slaves; x++) {
        if (srv.slaves[x].id == id) {
            sl = &srv.slaves[x];
            break;
        }
    }
    if (!sl) {
        send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }
    int len =
        sprintf(reply,
            "<slave-info for=\"%s-%d\">"
              "<address>%s:%hd</address>"
            "</slave-info>",
            sl->name,
            sl->id,
            (sl->ready ? sl->listen_addr : "0"),
            (sl->ready ? sl->listen_port : 0)
            );
    x = sprintf(tmp, "100 %d\n", len);
    send(conn->sock, tmp, x, 0);
    send(conn->sock, reply, len, 0);
    return 0;
}

/** 
 * The CLIENT-INFO outputs information about a client
 * in XML format. Syntax:
 * CLIENT-INFO <client>\n
 * 'client' should be the NAME of a client
 **/
static int
user_client_info_command(nolp_t *no, char *buf, int size)
{
    int x, y;
    int  found;
    int  sz;
    char out[256];
    slave_conn_t  *sl;
    struct conn   *conn;
    client_conn_t *c;
    conn = (struct conn*)no->private;

    if (conn->level < NOL_LEVEL_READ) {
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    if (size != 40)
        return -1; /* return -1 to close the connection */

    found = 0;
    c = 0;

    /* search through all slaves for a client matching the 
     * given hash */
    for (x=0; x<srv.num_slaves; x++) {
        for (y=0; y<srv.slaves[x].num_clients; y++) {
            if (memcmp(srv.slaves[x].clients[y].token, buf, 40) == 0) {
                c = &srv.slaves[x].clients[y];
                sl = &srv.slaves[x];
                found = 1;
                break;
            }
        }
        if (found)
            break;
    }
    if (!c) {
        /* send 203 not found message if the client was not
         * found */
        send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    sz = sprintf(out, 
            "<client id=\"%.40s\">"
              "<user>%.64s</user>"
              "<slave>%.64s-%d</slave>"
              "<status>%u</status>"
              "<address>%.15s</address>"
              "<session>%u</session>"
            "</client>",
            c->token,
            c->user,
            sl->name, sl->id,
            (c->status & 1),
            c->addr, c->session_id);
    x = sprintf(out+sz, "100 %d\n", sz);

    send(conn->sock, out+sz, x, 0);
    send(conn->sock, out, sz, 0);
    return 0;
}

static int
user_show_config_command(nolp_t *no, char *buf, int size)
{
    char           out[64];
    struct conn   *conn;
    int            x;
    conn = (struct conn *)no->private;

    if (conn->level < NOL_LEVEL_ADMIN) {
        send(no->fd, MSG200, sizeof(MSG200)-1, 0);
        return 0;
    }

    x = sprintf(out, "100 %d\n", srv.config_sz);
    send(conn->sock, out, x, 0);
    send(conn->sock, srv.config_buf, srv.config_sz, 0);
    return 0;
}

static int
user_log_command(nolp_t *no, char *buf, int size)
{
    return 0;
}

/** 
 * called when the ADD request is performed by
 * a user. ADD is used to add URLs.
 **/
static int
user_add_command(nolp_t *no, char *buf, int size)
{
    char q[size+32];
    char crawler[64];
    char *e;
    int  len;
    int  x;
    struct conn *conn = (struct conn *)no->private;

    if (conn->level < NOL_LEVEL_WRITE) {
        send(no->fd, MSG200, sizeof(MSG200)-1, 0);
        return 0;
    }

    e = buf+size;
    buf[size] = '\0';
    if (!(len = sscanf(buf, "%64s", &crawler)))
        return -1;
    while (!isspace(*buf) && buf<e)
        buf++;
    while (isspace(*buf) && buf<e)
        buf++;
    /* make sure there's no single-quote that can 
     * risk injecting SQL */
    strrmsq(crawler);
    strrmsq(buf);
    len = sprintf(q, "INSERT INTO nol_added (user_id, crawler, input, date) "
                     "VALUES (%d, '%s', '%s', NOW())",
                     ((struct conn*)no->private)->user_id,
                     crawler,
                     buf);
    if (mysql_real_query(srv.mysql, q, len) != 0) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }

    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;
}
/** 
 * Syntax:
 * <username>\n
 * <password>\n
 * <full-name>\n
 * <level>\n
 * <extra>
 **/
static int
user_useradd_recv(nolp_t *no, char *buf, int size)
{
    char *q, *q_start;
    int   sz;
    int level;
    char *username;
    int   u_len;
    char *password;
    int   p_len;
    char *fullname;
    int   f_len;
    char *extra;
    int   e_len;
    struct conn *conn;
    conn = (struct conn *)no->private;

    username = buf;

    if (!(password = memchr(buf, '\n', size)))
        goto invalid;
    *password = '\0';
    u_len = password-buf;
    password ++;

    if (!(fullname = memchr(password, '\n', size-(password-buf))))
        goto invalid;
    *fullname = '\0';
    p_len = fullname-password;
    fullname ++;

    if (!(extra = memchr(fullname, '\n', size-(fullname-buf))))
        goto invalid;
    f_len = extra-fullname;
    level = atoi(extra+1);
    if (!(extra = memchr(extra+1, '\n', size-(extra-buf)-1)))
        goto invalid;

    extra++;
    e_len = (buf+size)-extra;

    if (!(q = q_start = malloc(size*2 + 196))) {/* this should be sufficient :o */
        syslog(LOG_ERR, "out of mem");
        return -1;
    }

    q += sprintf(q, 
            "INSERT INTO `nol_user` (`user`, `pass`, `fullname`, `level`, `extra`) "
            "VALUES ('"
            );

    q += mysql_real_escape_string(srv.mysql, q, username, u_len);
    q += sprintf(q, "', MD5('");
    q += mysql_real_escape_string(srv.mysql, q, password, p_len);
    q += sprintf(q, "'), '");
    q += mysql_real_escape_string(srv.mysql, q, fullname, f_len);
    q += sprintf(q, "', %d, '", level);
    q += mysql_real_escape_string(srv.mysql, q, extra, e_len);
    q += sprintf(q, "')");

    if (mysql_real_query(srv.mysql, q_start, q-q_start) != 0) {
        syslog(LOG_ERR, "adding user failed: %s", mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        free(q_start);
        return -1;
    }

    syslog(LOG_INFO, "#%d added new user `%s`, level %d", no->fd, username, level);
    free(q_start);
    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;

invalid:
    syslog(LOG_ERR, "incorrect USERADD buffer syntax");
    send(no->fd, MSG201, sizeof(MSG201)-1, 0);
    return -1;
}


/** 
 * USERADD <bufsz>\n
 **/
static int
user_useradd_command(nolp_t *no, char *buf, int size)
{
    int sz;
    struct conn *conn;
    conn = (struct conn *)no->private;

    if (conn->level < NOL_LEVEL_MANAGER) {
        send(no->fd, MSG200, sizeof(MSG200)-1, 0);
        return 0;
    }

    if (!(sz = atoi(buf)))
        return -1;
    return nolp_expect(no, sz, &user_useradd_recv);
}

/** 
 * USERDEL <id>\n
 **/
static int
user_userdel_command(nolp_t *no, char *buf, int size)
{
    struct conn *conn;
    char q[128];
    int  sz;
    int user_id;
    conn = (struct conn *)no->private;
    if (conn->level < NOL_LEVEL_MANAGER) {
        send(no->fd, MSG200, sizeof(MSG200)-1, 0);
        return 0;
    }

    user_id = atoi(buf);

    sz = sprintf(q, "UPDATE `nol_user` SET deleted=1 WHERE id=%d", user_id);
    if (mysql_real_query(srv.mysql, q, sz) != 0) {
        syslog(LOG_ERR, "could not delete user with id %d: %s", user_id, mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return 0;
    }

    if (mysql_affected_rows(srv.mysql) < 1) {
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;
}

/** 
 * Set the users own password
 *
 * PASSWD <new-password>
 **/
static int
user_passwd_command(nolp_t *no, char *buf, int size)
{
    struct conn *conn;
    conn = (struct conn *)no->private;
    char *user;
    char q[size+64];
    char *pwd_escaped;
    int  sz;

    if (!(pwd_escaped = malloc(size*2+1))) {
        syslog(LOG_ERR, "out of mem");
        return -1;
    }

    mysql_real_escape_string(srv.mysql, pwd_escaped, buf, size);

    /* chagen the current users passsword */
    sz = sprintf(q, "UPDATE `nol_user` SET pass=MD5('%s') WHERE id=%d",
                 pwd_escaped, conn->user_id);
    free(pwd_escaped);

    if (mysql_real_query(srv.mysql, q, sz) != 0) {
        syslog(LOG_ERR, "PASSWD failed: %s", mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }
    if (mysql_affected_rows(srv.mysql) < 1) {
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

#ifdef DEBUG
    syslog(LOG_DEBUG, "#%d set own passwd", no->fd);
#endif
    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;
}
/** 
 * Set the password for the user with the given id
 *
 * PASSWD-ID <id> <new-password>\n
 **/
static int
user_passwd_id_command(nolp_t *no, char *buf, int size)
{
    struct conn *conn;
    conn = (struct conn *)no->private;
    char *pwd;
    char q[size+64];
    char *pwd_escaped;
    int  sz;
    int  user_id;

    if (conn->level < NOL_LEVEL_MANAGER) {
        send(no->fd, MSG200, sizeof(MSG200)-1, 0);
        return 0;
    }

    if (!(pwd = memrchr(buf, ' ', size))) {
        send(no->fd, MSG201, sizeof(MSG201)-1, 0);
        return -1;
    }

    while (isspace(*pwd)) pwd ++;
    user_id = atoi(buf);
    sz = size-(pwd-buf);
    while (isspace(pwd[sz-1])) sz--;
    pwd[sz] = '\0';

    if (!(pwd_escaped = malloc(sz*2+1))) {
        syslog(LOG_ERR, "out of mem");
        return -1;
    }

    mysql_real_escape_string(srv.mysql, pwd_escaped, pwd, sz);

    sz = sprintf(q, "UPDATE `nol_user` SET pass=MD5('%s') WHERE id=%d",
                 pwd_escaped, user_id);
    free(pwd_escaped);

    if (mysql_real_query(srv.mysql, q, sz) != 0) {
        syslog(LOG_ERR, "PASSWD failed: %s", mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }

    if (mysql_affected_rows(srv.mysql) < 1) {
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

#ifdef DEBUG
    syslog(LOG_DEBUG, "#%d set passwd for id %d", no->fd, user_id);
#endif
    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;
}

#define BUF_SZ 512
#define BUF_CHK(x) { \
        while (b_sz+(x) > b_cap) {\
            b_cap += BUF_SZ; \
            if (!(b_ptr = realloc(b_ptr, b_cap))) \
                return -1;\
        }\
    }
#define BUF_ADD(_s, _x) {\
        int _z; \
        if (!_x) {\
            _z = strlen(_s); \
            BUF_CHK(_z); \
            memcpy(b_ptr+b_sz, _s, _z); \
        } else {\
            BUF_CHK(_x); \
            memcpy(b_ptr+b_sz, _s, _x); \
            _z = _x; \
        } \
        b_sz += _z; \
    }

static int
user_session_info_command(nolp_t *no, char *buf, int size)
{
    uint32_t session_id;
    session_id = atoi(buf);
    unsigned long *lengths;
    MYSQL_RES *r;
    MYSQL_ROW row;
    MYSQL_FIELD *fields;
    unsigned int num;
    unsigned int b_sz, b_cap;
    unsigned int x;
    char *b_ptr;

    if (!(b_ptr = malloc(BUF_SZ)))
        return -1;
    b_cap = BUF_SZ;
    b_sz  = 0;

    /* create the query, first we need to find out what count_* columns we have */
    mysql_query(srv.mysql, "SHOW COLUMNS FROM `nol_session` WHERE `Field` LIKE 'count_%';");
    if (!(r = mysql_store_result(srv.mysql))) {
        syslog(LOG_ERR, "could not get column names: %s",
                mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }
    BUF_ADD("SELECT ", 0);
    while (row = mysql_fetch_row(r)) {
        BUF_CHK(strlen("`nol_session`.``, as 'num-'")+strlen(row[0])*2);
        b_sz += sprintf(b_ptr+b_sz,
                "`nol_session`.`%s` as 'num-%s', ",
                row[0], row[0]+6);
    }
    mysql_free_result(r);

    BUF_ADD(
            "`cl`.`token` as client, `a`.`crawler` as crawler, `a`.`input` as input, "
            "`nol_session`.`date` as started, `nol_session`.`latest` as updated, "
            "`nol_session`.`state` as state "
            "FROM "
                "`nol_session` "
            "LEFT JOIN "
                "`nol_client` as `cl` "
              "ON "
                "`cl`.`id` = `nol_session`.`client_id` "
            "LEFT JOIN "
                "`nol_added` as `a` "
              "ON "
                "`a`.`id` = `nol_session`.`added_id` "
            "WHERE `nol_session`.`id` = ", 0);
    BUF_CHK(25);
    b_sz += sprintf(b_ptr+b_sz, "%u LIMIT 0,1;", session_id);

    if (mysql_real_query(srv.mysql, b_ptr, b_sz) != 0) {
        syslog(LOG_ERR, "selecting session info failed: %s",
                mysql_error(srv.mysql));
        free(b_ptr);
        return -1;
    }

    r = mysql_store_result(srv.mysql);
    if (!r) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        free(b_ptr);
        return -1;
    }
    if (!(row = mysql_fetch_row(r))) {
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);
        free(b_ptr);
        return 0;
    }
    if (!(lengths = mysql_fetch_lengths(r))
        || !(fields = mysql_fetch_fields(r))) {
        mysql_free_result(r);
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        free(b_ptr);
        return -1;
    }

    num = mysql_field_count(srv.mysql);

    /* reset the buffer */
    b_sz = 0;

    BUF_ADD("<session-info for=\"", 0);
    BUF_CHK(15);
    b_sz += sprintf(b_ptr+b_sz, "%u\">", session_id);

    for (x=0; x<num; x++) {
        BUF_CHK(fields[x].name_length*2 + lengths[x] + 5);
        b_sz += sprintf(b_ptr+b_sz, "<%s>%s</%s>",
                fields[x].name,
                row[x]?row[x]:"",
                fields[x].name);
    }
    BUF_ADD("</session-info>", 0);
    BUF_CHK(20);
    x = sprintf(b_ptr+b_sz, "100 %u\n", b_sz);

    mysql_free_result(r);

    if (send(no->fd, b_ptr+b_sz, x, 0) <= 0
        || send(no->fd, b_ptr, b_sz, 0) <= 0) {
        free(b_ptr);
        return -1;
    }

    free(b_ptr);
    return 0;
}

static int
user_session_report_command(nolp_t *no, char *buf, int size)
{
    MYSQL_RES *r;
    MYSQL_ROW row;
    char b[56+12];
    int  sz;
    unsigned long *lengths;

    sz = sprintf(b,
            "SELECT `report` FROM `nol_session` "
            "WHERE `id`=%d LIMIT 0,1",
            atoi(buf));
    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "fetching session report failed: %s", 
                mysql_error(srv.mysql));
        return -1;
    }
    if (!(r = mysql_store_result(srv.mysql))) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }
    if (row = mysql_fetch_row(r)) {
        lengths = mysql_fetch_lengths(r);
        sz = sprintf(b, "100 %d\n", (int)lengths[0]);
        send(no->fd, b, sz, 0);
        send(no->fd, row[0], (int)lengths[0], 0);
    } else
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);

    mysql_free_result(r);
    return 0;
}

/** 
 * SESSION-LIST <start> <count>\n
 **/
static int
user_list_sessions_command(nolp_t *no, char *buf, int size)
{
    int start, limit;
    char *b;
    MYSQL_RES *r;
    MYSQL_ROW row;
    int sz;

    if (sscanf(buf, "%d %d", &start, &limit) != 2) {
        send(no->fd, MSG201, sizeof(MSG201)-1, 0);
        return -1;
    }
    if (limit > 100) limit = 100;
    char **bufs = malloc(sizeof(char *)*limit);
    int   *sizes = malloc(sizeof(int)*limit);

    sz = asprintf(&b,
            "SELECT "
                "`S`.`id`, `S`.`latest`, `S`.`state`, "
                "`A`.crawler, `A`.input , `C`.`token`"
            "FROM `nol_session` as `S` "
            "LEFT JOIN "
                "`nol_added` as `A` "
              "ON "
                "`A`.`id` = `S`.`added_id` "
            "LEFT JOIN "
                "`nol_client` AS `C` "
              "ON "
                "`C`.`id` = `S`.`client_id` "
            "ORDER BY "
                "`S`.`latest` DESC "
            "LIMIT %d, %d;",
            start, limit);

    if (!b) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        syslog(LOG_ERR, "out of mem");
        return -1;
    }
    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "LIST-SESSIONS failed: %s",
                mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        free(b);
        return -1;
    }
    free(b);

    if ((r = mysql_store_result(srv.mysql))) {
        int x = 0;
        int total = 0;
        while (row = mysql_fetch_row(r)) {
            unsigned long *l;
            l = mysql_fetch_lengths(r);
            char *curr = malloc(l[0]+l[1]+l[2]+l[3]+l[4]+l[5]+
                    sizeof("<session id=\"\"><latest></latest><state></state><crawler></crawler><input></input><client></client></session>"));
            sz = sprintf(curr,
                    "<session id=\"%d\">"
                      "<latest>%s</latest>"
                      "<state>%s</state>"
                      "<crawler>%s</crawler>"
                      "<input>%s</input>"
                      "<client>%s</client>"
                    "</session>",
                    atoi(row[0]), row[1]?row[1]:"", row[2]?row[2]:"",
                    row[3]?row[3]:"", row[4]?row[4]:"", row[5]?row[5]:"");

            bufs[x] = curr;
            sizes[x] = sz;
            total += sz;
            x++;
        }
        limit = x;

        char *tmp;
        sz = asprintf(&tmp, "100 %d\n", total+sizeof("<session-list></session-list>")-1);
        send(no->fd, tmp, sz, 0);
        free(tmp);

        send(no->fd, "<session-list>", sizeof("<session-list>")-1, 0);
        for (x=0; x<limit; x++) {
            send(no->fd, bufs[x], sizes[x], 0);
            free(bufs[x]);
        }
        send(no->fd, "</session-list>", sizeof("</session-list>")-1, 0);
    } else 
        syslog(LOG_ERR, "mysql_store_result() failed");
    free(bufs);
    free(sizes);

    return 0;
}

/** 
 * LIST-USERS <start> <count>\n
 **/
static int
user_list_users_command(nolp_t *no, char *buf, int size)
{
    int start, limit;
    char *b;
    MYSQL_RES *r;
    MYSQL_ROW row;
    int sz;
    struct conn *conn = (struct conn*)no->private;

    if (conn->level < NOL_LEVEL_MANAGER) {
        send(no->fd, MSG200, sizeof(MSG200)-1, 0);
        return 0;
    }

    if (sscanf(buf, "%d %d", &start, &limit) != 2) {
        send(no->fd, MSG201, sizeof(MSG201)-1, 0);
        return -1;
    }
    if (limit > 100) limit = 100;
    char **bufs = malloc(sizeof(char *)*limit);
    int   *sizes = malloc(sizeof(int)*limit);

    sz = asprintf(&b,
            "SELECT "
                "`id`, `user`, `fullname`, `extra`, `level` "
            "FROM `nol_user`"
            "WHERE deleted=0 "
            "ORDER BY "
                "`id` DESC "
            "LIMIT %d, %d;",
            start, limit);

    if (!b) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        syslog(LOG_ERR, "out of mem");
        return -1;
    }
    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "LIST-USERS failed: %s",
                mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        free(b);
        return -1;
    }
    free(b);

    if ((r = mysql_store_result(srv.mysql))) {
        int x = 0;
        int total = 0;
        while (row = mysql_fetch_row(r)) {
            unsigned long *l;
            l = mysql_fetch_lengths(r);
            char *curr = malloc(l[0]+l[1]+l[2]+l[3]+l[4]+
                    sizeof("<user id=\"\"><username></username><fullname></fullname><extra></extra><level></level></user>"));
            sz = sprintf(curr,
                    "<user id=\"%d\">"
                      "<username>%s</username>"
                      "<fullname>%s</fullname>"
                      "<extra>%s</extra>"
                      "<level>%s</level>"
                    "</user>",
                    atoi(row[0]), row[1]?row[1]:"", row[2]?row[2]:"",
                    row[3]?row[3]:"",
                    row[4]?row[4]:""
                    );

            bufs[x] = curr;
            sizes[x] = sz;
            total += sz;
            x++;
        }
        limit = x;

        char *tmp;
        sz = asprintf(&tmp, "100 %d\n", total+sizeof("<user-list></user-list>")-1);
        send(no->fd, tmp, sz, 0);
        free(tmp);

        send(no->fd, "<user-list>", sizeof("<user-list>")-1, 0);
        for (x=0; x<limit; x++) {
            send(no->fd, bufs[x], sizes[x], 0);
            free(bufs[x]);
        }
        send(no->fd, "</user-list>", sizeof("</user-list>")-1, 0);
    } else 
        syslog(LOG_ERR, "mysql_store_result() failed");
    free(bufs);
    free(sizes);

    return 0;
}

/** 
 * LIST-INPUT\n
 *
 * list the input data by the current user
 **/
static int
user_list_input_command(nolp_t *no, char *buf, int size)
{
    MYSQL_RES *r;
    MYSQL_ROW row;
    char b[256];
    struct conn *conn = (struct conn*)no->private;
    int   sz;
    long  count;

    sz = sprintf(b, "SELECT nol_added.id, crawler, input, S.id, S.latest FROM nol_added LEFT JOIN `nol_session` as S ON S.added_id = nol_added.id WHERE user_id=%d ORDER BY `nol_added`.`id` DESC LIMIT 0,1000", conn->user_id);

    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "LIST-INPUT failed: %s",
                mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }
    char **bufs;
    int   *sizes;
    if ((r = mysql_store_result(srv.mysql))) {
        count = (long)mysql_num_rows(r);
        bufs = malloc(sizeof(char *)*count);
        sizes = malloc(sizeof(int)*count);
        int x = 0;
        int total = 0;
        if (!bufs || !sizes)
            return -1;
        while (row = mysql_fetch_row(r)) {
            unsigned long *l;
            l = mysql_fetch_lengths(r);
            char *curr = malloc(l[0]+l[1]+l[2]+l[3]+l[4]+
                    sizeof("<input id=\"\"><crawler></crawler><value></value><latest-session></latest-session><latest-session-date></latest-session-date></input>"));
            sz = sprintf(curr,
                    "<input id=\"%d\">"
                      "<crawler>%s</crawler>"
                      "<value>%s</value>"
                      "<latest-session>%s</latest-session>"
                      "<latest-session-date>%s</latest-session-date>"
                    "</input>",
                    atoi(row[0]), row[1]?row[1]:"",
                    row[2]?row[2]:"",
                    row[3]?row[3]:"",
                    row[4]?row[4]:""
                    );
            bufs[x] = curr;
            sizes[x] = sz;
            total += sz;
            x++;
        }
        count = (long)x;

        char *tmp;
        sz = asprintf(&tmp, "100 %u\n", (unsigned int)total+sizeof("<input-list></input-list>")-1);
        send(no->fd, tmp, sz, 0);
        free(tmp);

        send(no->fd, "<input-list>", sizeof("<input-list>")-1, 0);
        for (x=0; x<count; x++) {
            send(no->fd, bufs[x], sizes[x], 0);
            free(bufs[x]);
        }
        send(no->fd, "</input-list>", sizeof("</input-list>")-1, 0);
    } else 
        syslog(LOG_ERR, "mysql_store_result() failed");
    free(bufs);
    free(sizes);

    return 0;
}

/** 
 * KILL-ALL <slave>\n
 *
 * Disconnects all clients connected to the given slave 
 * TODO: check permissions
 **/
static int
user_kill_all_command(nolp_t *no, char *buf, int size)
{
    int           id;
    int           x;
    slave_conn_t *sl = 0;

    id = atoi(buf);
    for (x=0; x<srv.num_slaves; x++) {
        if (srv.slaves[x].id == id) {
            sl = &srv.slaves[x];
            break;
        }
    }
    if (!sl) {
        send(sl->conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    send(sl->conn->sock, "KILL-ALL\n", 9, 0);
    return 0;
}

/** 
 * SYSTEM-INFO command is used to get an overview of
 * the running system
 * XML:
 *
 * <system-info>
 *   <uptime>... seconds since master started ...</uptime>
 *   <address>... listening address of master ...</address>
 *   <num-slaves>... num connected slaves ...</num-slaves>
 *   <num-sessions>... total session count ...</num-sessions>
 *   <num-users>... total user count ...</num-sessions>
 * </system-info>
 **/
static int
user_system_info_command(nolp_t *no, char *buf, int size)
{
    MYSQL_RES *res;
    MYSQL_ROW  row;
    time_t now;
    double uptime;
    int  sz, sz2;
    int  num_sessions = 0;
    int  num_users = 0;
    char xml[sizeof("<system-info><uptime></uptime><address></address>"
                    "<num-slaves></num-slaves>"
                    "<num-sessions></num-sessions>"
                    "<num-users></num-users>"
                    "</system-info>")
                    +128];
    time(&now);
    uptime = difftime(now, srv.start_time);

    mysql_query(srv.mysql, "SELECT COUNT(*) FROM `nol_session`;");
    if (!(res = mysql_store_result(srv.mysql)))
        return -1;
    if (!(row = mysql_fetch_row(res))) {
        mysql_free_result(res);
        return -1;
    }
    num_sessions = atoi(row[0]);
    mysql_free_result(res);

    mysql_query(srv.mysql, "SELECT COUNT(*) FROM `nol_user`;");
    if (!(res = mysql_store_result(srv.mysql)))
        return -1;
    if (!(row = mysql_fetch_row(res))) {
        mysql_free_result(res);
        return -1;
    }
    num_users = atoi(row[0]);
    mysql_free_result(res);

    sz = sprintf(xml, "<system-info>"
                         "<uptime>%ld</uptime>"
                         "<address>%.15s:%hd</address>"
                         "<num-slaves>%d</num-slaves>"
                         "<num-sessions>%d</num-sessions>"
                         "<num-users>%d</num-users>"
                      "</system-info>",
                      (long)uptime,
                      inet_ntoa(srv.addr.sin_addr),
                      htons(srv.addr.sin_port),
                      srv.num_slaves,
                      num_sessions, num_users);
    sz2 = sprintf(xml+sz, "100 %d\n", sz);
    send(no->fd, xml+sz, sz2, 0);
    send(no->fd, xml, sz, 0);
    return 0;
}

/** 
 * Syntax:
 * HELLO\n
 *
 * Returns:
 * 100 <buf-size>\n
 * <hello>
 *   <num-messages></num-messages>
 *   <user-level></user-level>
 * </hello>
 **/
static int
user_hello_command(nolp_t *no, char *buf, int size)
{
    char out[6+(15*3)+sizeof("<hello><num-messages></num-messages><user-level></user-level></hello>")-1];
    struct conn *conn = (struct conn*)no->private;
    int  sz, sz2;

    sz = sprintf(out, "<hello><num-messages>%d</num-messages><user-level>%d</user-level></hello>",
            0, conn->level);
    sz2 = sprintf(out+sz, "100 %d\n", sz);

    send(no->fd, out+sz, sz2, 0);
    send(no->fd, out, sz, 0);

    return 0;
}

