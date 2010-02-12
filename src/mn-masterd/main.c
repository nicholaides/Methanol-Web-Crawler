/*-
 * main.c
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <ev.h>

#include "master.h"
#include "conn.h"
#include "conf.h"
#include "slave.h"
#include "client.h"

int nol_m_main();
int nol_m_cleanup();
static void nol_m_ev_sigint(EV_P_ ev_signal *w, int revents);
static void nol_m_ev_sigterm(EV_P_ ev_signal *w, int revents);
const char* master_init_cb(void);
const char* master_start_cb(void);
const char* load_hooks();
void free_hooks();

static const char *_cfg_file;
struct master       srv;
struct opt_val_list opt_vals;

struct lmc_scope master_scope =
{
    "master",
    {
        LMC_OPT_STRING("listen", &opt_vals.listen),
        LMC_OPT_STRING("config_file", &opt_vals.config_file),
        LMC_OPT_STRING("session_complete_hook", &opt_vals.hooks[HOOK_SESSION_COMPLETE]),
        LMC_OPT_STRING("cleanup_hook", &opt_vals.hooks[HOOK_CLEANUP]),
        LMC_OPT_STRING("user", &opt_vals.user),
        LMC_OPT_STRING("group", &opt_vals.group),
        LMC_OPT_STRING("mysql_host", &opt_vals.mysql_host),
        LMC_OPT_STRING("mysql_sock", &opt_vals.mysql_sock),
        LMC_OPT_STRING("mysql_user", &opt_vals.mysql_user),
        LMC_OPT_STRING("mysql_pass", &opt_vals.mysql_pass),
        LMC_OPT_STRING("mysql_db", &opt_vals.mysql_db),
        LMC_OPT_UINT("mysql_port", &opt_vals.mysql_port),
        LMC_OPT_END,
    }
};

int
main(int argc, char **argv)
{
    lmc_parser_t *lmc;
    int           r;
    int           x;
    int           nofork = 0;
    signal(SIGPIPE, SIG_IGN);

    _cfg_file = "/etc/mn-masterd.conf";
    if (argc > 1) {
        for (x=1; x<argc; x++) {
            if (strcmp(argv[x], "--no-fork") == 0)
                nofork=1;
            else
                _cfg_file = argv[x];
        }
    }

    time(&srv.start_time);

    if (!(lmc = lmc_create(&srv))) {
        fprintf(stderr, "out of mem\n");
        return 1;
    }

    lmc_add_scope(lmc, &master_scope);
    lmc_add_class(lmc, &nol_slave_class);
    lmc_add_class(lmc, &nol_client_class);

    if ((r = nol_daemon_launch(
                _cfg_file,
                lmc,
                &opt_vals.user,
                &opt_vals.group,
                &master_init_cb,
                &master_start_cb,
                nofork==1?0:1)) == 0)
        fprintf(stdout, "started\n");

    lmc_destroy(lmc);
    nol_m_cleanup();
    return r;
}

/* initialise the server, return 0 on success or a pointer
 * to a static error buffer on failure
 *
 * this will be run by the forked process, and called from
 * nol_daemon_launch()
 * */
const char *
master_init_cb(void)
{
    FILE *fp;
    char *s;
    int o = 1, sock;
    openlog("mn-masterd", LOG_PID, LOG_DAEMON);

    if (!srv.auth.num_slaves) {
        syslog(LOG_WARNING, "No slave login declared, using default:default!");
        srv.auth.num_slaves = 1;
        if (!(srv.auth.slaves = calloc(1, sizeof(slave_t*)))
                || !(srv.auth.slaves[0] = calloc(1, sizeof(slave_t))))
            return "out of mem";
        slave_t *sl = srv.auth.slaves[0];
        if (!(sl->name = strdup("default")))
            return "out of mem";
        if (!(sl->password = strdup("default")))
            return "out of mem";
    }
    if (!srv.auth.num_clients) {
        syslog(LOG_WARNING, "No client login declared, using default:default!");
        srv.auth.num_clients = 1;
        if (!(srv.auth.clients = calloc(1, sizeof(client_t*)))
                || !(srv.auth.clients[0] = calloc(1, sizeof(client_t))))
            return "out of mem";
        client_t *cl = srv.auth.clients[0];
        if (!(cl->name = strdup("default")))
            return "out of mem";
        if (!(cl->password = strdup("default")))
            return "out of mem";
    }

    if (!opt_vals.config_file)
        return "no configuration file";

    if ((s = strchr(opt_vals.listen, ':'))) {
        srv.addr.sin_port = htons(atoi(s+1));
        *s = '\0';
    } else
        srv.addr.sin_port = htons(NOL_MASTER_DEFAULT_PORT);
    srv.addr.sin_addr.s_addr = inet_addr(opt_vals.listen);
    srv.addr.sin_family = AF_INET;

    /* load the configuration file */
    if (!(fp = fopen(opt_vals.config_file, "rb")))
        return "could not open configuration file";

    /* calculate the file length */
    fseek(fp, 0, SEEK_END);
    srv.config_sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (!(srv.config_buf = malloc(srv.config_sz))) {
        fclose(fp);
        return "out of mem";
    }

    if ((size_t)srv.config_sz != fread(srv.config_buf, 1, srv.config_sz, fp)) {
        fclose(fp);
        return "read error";
    }

    fclose(fp);
    fp = 0;

    if (nol_m_mysql_connect() != 0)
        return "could not connect to mysql server";
    if (nol_m_mysql_setup() != 0)
        return "settings up mysql tables failed";
    if (nol_m_reconfigure() != 0)
        return "settings up filetype tables failed";

    sock = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o));

    if (bind(sock, (struct sockaddr*)&srv.addr, sizeof srv.addr) == -1) {
        syslog(LOG_ERR, "could not bind to %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return "bind() failed";
    }
    if (listen(sock, 1024) == -1) {
        syslog(LOG_ERR, "could not listen on %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return "listen() failed";
    }

    srv.listen_sock = sock;
    return load_hooks();
}

/** 
 * load all set hooks into memory, so we can send them to 
 * connecting slaves
 *
 * return 0 or a static char buffer describing the error
 **/
const char*
load_hooks()
{
    int     x;
    FILE  *fp;
    size_t length;

    for (x=0; x<NUM_HOOKS; x++) {
        if (opt_vals.hooks[x]) {
            if (!(fp = fopen(opt_vals.hooks[x], "r")))
                return "could not open hook script";
            fseek(fp, 0, SEEK_END);
            length = (int)ftell(fp);
            rewind(fp);

            if (!(srv.hooks[x].buf = malloc(length)))
                return "out of mem";
            srv.hooks[x].sz = length;
            fread(srv.hooks[x].buf, 1, length, fp);
            fclose(fp);
        }
    }

    return 0;
}

void
free_hooks()
{
    int x;
    for (x=0; x<NUM_HOOKS; x++) {
        if (srv.hooks[x].sz) {
            free(srv.hooks[x].buf);
            srv.hooks[x].sz = 0;
        }
    }
}

/** 
 * start the event loop and begin accepting
 * connections
 *
 * called by nol_daemon_launch()
 **/
const char*
master_start_cb()
{
    struct ev_loop *loop;
    ev_signal       sigint_listen;
    ev_signal       sigterm_listen;
    ev_io           io_listen;
    
    if (!(loop = ev_default_loop(EVFLAG_AUTO)))
        return 1;

    syslog(LOG_INFO, "listening on %s:%hd",
            inet_ntoa(srv.addr.sin_addr),
            ntohs(srv.addr.sin_port));

    ev_signal_init(&sigint_listen,
            &nol_m_ev_sigint, SIGINT);
    ev_signal_init(&sigterm_listen,
            &nol_m_ev_sigterm, SIGTERM);
    ev_io_init(&io_listen, &nol_m_ev_conn_accept,
            srv.listen_sock, EV_READ);

    /* catch SIGINT */
    ev_signal_start(loop, &sigint_listen);
    ev_signal_start(loop, &sigterm_listen);
    ev_io_start(loop, &io_listen);

    ev_loop(loop, 0);
    close(srv.listen_sock);
    ev_default_destroy();

    closelog();
    return 0;
}

int
nol_m_mysql_connect()
{
    my_bool reconnect = 1;
    if (!(srv.mysql = mysql_init(0)))
        return -1;
    mysql_options(srv.mysql, MYSQL_OPT_RECONNECT, &reconnect);
    if (!(mysql_real_connect(srv.mysql,
                    opt_vals.mysql_host, opt_vals.mysql_user,
                    opt_vals.mysql_pass, opt_vals.mysql_db,
                    opt_vals.mysql_port, opt_vals.mysql_sock,
                    0))) {
        mysql_close(srv.mysql);
        return 0;
    }

    return 0;
}

#define SQL_USER_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_user ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    user VARCHAR(32) UNIQUE, pass VARCHAR(32), \
                    `fullname` VARCHAR(128),\
                    level INT NOT NULL,\
                    extra VARCHAR(128), \
                    deleted INT(1) NOT NULL DEFAULT 0, \
                    PRIMARY KEY (id))"
#define SQL_CLIENT_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_client ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    token VARCHAR(40), \
                    PRIMARY KEY (id) \
                    )"
#define SQL_SLAVE_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_slave ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    user VARCHAR(64), \
                    PRIMARY KEY (id) \
                    )"
#define SQL_URL_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_url ( \
                    hash VARCHAR(40), \
                    url VARCHAR(4096), \
                    date DATETIME, \
                    PRIMARY KEY (hash) \
                    )"
#define SQL_ADDED_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_added ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    user_id INT, \
                    crawler VARCHAR(64), \
                    input VARCHAR(4096), \
                    date DATETIME, \
                    PRIMARY KEY (id),\
                    INDEX (user_id)\
                    )"
#define SQL_MSG_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_msg ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    `to` INT, \
                    `from` INT, \
                    `date` DATETIME, \
                    title VARCHAR(255), \
                    content TEXT, \
                    PRIMARY KEY (id), \
                    INDEX (`to`) \
                    )"
#define SQL_SESS_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_session ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    `added_id` INT, \
                    `client_id` INT, \
                    `date` DATETIME, \
                    `latest` DATETIME, \
                    `state` ENUM('running','interrupted','hook','done') DEFAULT 'running', \
                    `report` TEXT, \
                    PRIMARY KEY (id)\
                    )"
#define SQL_SESS_REL_UNIQ "\
            ALTER TABLE nol_session_rel ADD UNIQUE (\
                `session_id`, \
                `target_id` \
            );"
#define SQL_SESS_REL_TBL "\
            CREATE TABLE \
            nol_session_rel ( \
                    session_id INT NOT NULL, \
                    filetype VARCHAR(64), \
                    `target_id` INT NOT NULL, \
                    INDEX (session_id)\
                    )"

/*
#define SQL_CRAWLERS_DROP "DROP TABLE IF EXISTS nol_crawlers;"
#define SQL_CRAWLERS_TBL "\
            CREATE TABLE \
            nol_crawlers ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    `to` INT, \
                    `from` INT, \
                    `date` DATETIME, \
                    title VARCHAR(255), \
                    content TEXT, \
                    PRIMARY KEY (id), \
                    INDEX (`to`) \
                    )"
                    */
#define SQL_USER_CHECK "SELECT null FROM nol_user LIMIT 0,1;"
#define SQL_ADD_DEFAULT_USER \
    "INSERT INTO nol_user (user, pass, fullname, level) VALUES ('admin@localhost', MD5('admin'), 'Administrator', 8192)"
/** 
 * Set up all default tables
 **/
int
nol_m_mysql_setup()
{
    MYSQL_RES *r;
    long       id;
    char msg[512];
    if (mysql_real_query(srv.mysql, SQL_USER_TBL, sizeof(SQL_USER_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating user table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_CLIENT_TBL, sizeof(SQL_CLIENT_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating client table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_SLAVE_TBL, sizeof(SQL_SLAVE_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating slave table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_URL_TBL, sizeof(SQL_URL_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating url table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_ADDED_TBL, sizeof(SQL_ADDED_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating input/added table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_MSG_TBL, sizeof(SQL_MSG_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating session table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_SESS_TBL, sizeof(SQL_SESS_TBL)-1) != 0) {
        syslog(LOG_ERR, "error creating session table: %s", mysql_error(srv.mysql));
        return -1;
    }
    if (mysql_real_query(srv.mysql, SQL_SESS_REL_TBL,
            sizeof(SQL_SESS_REL_TBL)-1) == 0) {
        mysql_real_query(srv.mysql, SQL_SESS_REL_UNIQ,
                sizeof(SQL_SESS_REL_UNIQ)-1);
    }

    /* if there's no user added to the user table, then we
     * must add the default one with admin:admin as login */
    if (mysql_real_query(srv.mysql, SQL_USER_CHECK, sizeof(SQL_USER_CHECK)-1) != 0) {
        syslog(LOG_ERR, "mysql error: %s\n", mysql_error(srv.mysql));
        return -1;
    }
    if (!(r = mysql_store_result(srv.mysql))) {
        syslog(LOG_ERR, "mysql error: %s\n", mysql_error(srv.mysql));
        return -1;
    }
    if (!mysql_num_rows(r)) {
        mysql_real_query(srv.mysql, SQL_ADD_DEFAULT_USER,
                sizeof(SQL_ADD_DEFAULT_USER)-1);
        id = mysql_insert_id(srv.mysql);
        /* also send a message to the admin user telling him to
         * change his password as soon as possible */
        int  len;
        len = sprintf(msg,
                "INSERT INTO nol_msg (`to`, `from`, `date`, `title`, `content`)"
                "VALUES (%d, NULL, NOW(), 'Password must be changed!', "
                "'You are currently logged in with the default "
                "account created on system startup. It is strongly "
                "recommended that you change your password.')",
                           (int)id);
        mysql_real_query(srv.mysql, msg, len);
    }
    mysql_free_result(r);

    return 0;
}

#define CREATE_TBL         "CREATE TABLE IF NOT EXISTS "
#define CREATE_TBL_LEN     (sizeof(CREATE_TBL)-1)
#define DEFAULT_LAYOUT     \
    "(id INT NOT NULL AUTO_INCREMENT, "\
    "url_hash VARCHAR(40), "\
    "date DATETIME, "\
    "PRIMARY KEY (id),"\
    "UNIQUE (url_hash))"
#define DEFAULT_LAYOUT_LEN (sizeof(DEFAULT_LAYOUT)-1)

enum {
    ATTR_TYPE_UNKNOWN,
    ATTR_TYPE_INT,
    ATTR_TYPE_TEXT,
    ATTR_TYPE_BLOB,
};

/** 
 * nol_m_reconfigure()
 * Create filetype tables.
 *
 * When a filetype is created, it will contain only one column
 * at first, and then ALTER TABLE is used to add the filetype's
 * attributes one by one. The benefit of adding the columns one,
 * by one is that whenever a filetype has been changed in the
 * configuration file, its new attributes will be added as new 
 * columns, while the existing columns will be untouched, thus
 * the system can easily reconfigure itself while retaining
 * already existing columns and data.
 *
 * No columns will be removed from filetype tables, since 
 * they might contain important data. It is up to the system
 * admin to remove unused columns.
 **/
int
nol_m_reconfigure()
{
    int  x, a;
    int  n;
    char tq[64+64+CREATE_TBL_LEN+DEFAULT_LAYOUT_LEN+1];
    char *name;
    int  len;
    lmc_parser_t *lmc;

    /* the lmc parser is created with 0 as root, since
     * our object-functions will modify the static 'srv'
     * directly */
    if (!(lmc = lmc_create(0)))
        return -1;

    lmc_add_class(lmc, &nol_m_filetype_class);
    lmc_add_class(lmc, &nol_m_crawler_class);
    if (lmc_parse_file(lmc, opt_vals.config_file) != M_OK) {
        fprintf(stderr, "%s", lmc->last_error);
        return -1;
    }

    for (n=0; n<srv.num_filetypes; n++) {
        name = srv.filetypes[n]->name;
        len = strlen(name);

        /** 
         * Make sure the name does not contain any weird
         * characters. Convert them to '_'.
         **/
        for (x=0; x<len; x++)
            if (!isalnum(*(name+x)) && *(name+x) != '_')
                *(name+x) = '_';

        len = sprintf(tq, "%sft_%.60s%s", 
                            CREATE_TBL,
                            name,
                            DEFAULT_LAYOUT
                );

        mysql_real_query(srv.mysql, tq, len);

        /* add this filetype as a counter column in the session table,
         * this query might fail but it doesnt matter, because then
         * the column probably exists already */
        len = sprintf(tq,
                "ALTER TABLE `nol_session` "
                "ADD COLUMN count_%.60s INT UNSIGNED",
                name);
        mysql_real_query(srv.mysql, tq, len);

        /** 
         * Now add all the columns. Many of these queries will probably
         * fail because the column already exists, but this is expected
         * behaviour.
         **/
        for (a=0; a<srv.filetypes[n]->num_attributes; a++) {
            char *attr = srv.filetypes[n]->attributes[a];
            char *type = strchr(attr, ' ');
            int type_n;

            if (!type || !strlen(type+1)) {
                syslog(LOG_ERR, "no type given for attribute '%s'", attr);
                continue;
            }
            type++;
            type_n = ATTR_TYPE_UNKNOWN;
            if (strcasecmp(type, "INT") == 0)
                type_n = ATTR_TYPE_INT;
            else if (strcasecmp(type, "TEXT") == 0)
                type_n = ATTR_TYPE_TEXT;
            else if (strcasecmp(type, "BLOB") == 0)
                type_n = ATTR_TYPE_BLOB;

            if (type_n == ATTR_TYPE_UNKNOWN) {
                syslog(LOG_ERR, "unknown attribute type '%s'", type);
                continue;
            }

            len = sprintf(tq, "ALTER TABLE ft_%.60s ADD COLUMN %.60s",
                    name, attr);
            if (mysql_real_query(srv.mysql, tq, len) == 0) {
                if (type_n == ATTR_TYPE_TEXT) {
                    *(type-1) = '\0';
                    sprintf(tq, "ALTER TABLE ft_%.60s ADD FULLTEXT (%.60s)",
                            name, attr);
                    mysql_real_query(srv.mysql, tq, len);
                }
            }

        }
    }

    lmc_destroy(lmc);

    return 0;
}

/** 
 * Called when sigint is recevied
 **/
static void
nol_m_ev_sigint(EV_P_ ev_signal *w, int revents)
{
    int x;

    ev_signal_stop(EV_A_ w);

    syslog(LOG_INFO, "SIGINT received");

    if (srv.num_conns) {
        for (x=0; x<srv.num_conns; x++) {
            close(srv.pool[x]->sock); 
            free(srv.pool[x]);
        }

        free(srv.pool);
    }

    ev_unloop(EV_A_ EVUNLOOP_ALL);
}

/** 
 * Called when sigterm is recevied
 **/
static void
nol_m_ev_sigterm(EV_P_ ev_signal *w, int revents)
{
    int x;

    ev_signal_stop(EV_A_ w);

    syslog(LOG_INFO, "SIGTERM received");

    if (srv.num_conns) {
        for (x=0; x<srv.num_conns; x++) {
            close(srv.pool[x]->sock); 
            free(srv.pool[x]);
        }

        free(srv.pool);
    }

    ev_unloop(EV_A_ EVUNLOOP_ALL);
}

int
nol_m_cleanup()
{
    int x;

    if (srv.num_filetypes) {
        for (x=0; x<srv.num_filetypes; x++)
            nol_m_filetype_destroy(srv.filetypes[x]);
        free(srv.filetypes);
    }

    if (srv.num_crawlers) {
        for (x=0; x<srv.num_crawlers; x++)
            nol_m_crawler_destroy(srv.crawlers[x]);
        free(srv.crawlers);
    }

    if (opt_vals.config_file)
        free(opt_vals.config_file);
    if (srv.config_sz)
        free(srv.config_buf);
    if (opt_vals.user)
        free(opt_vals.user);
    if (opt_vals.group)
        free(opt_vals.group);
    if (opt_vals.mysql_host) free(opt_vals.mysql_host);
    if (opt_vals.mysql_sock) free(opt_vals.mysql_sock);
    if (opt_vals.mysql_user) free(opt_vals.mysql_user);
    if (opt_vals.mysql_pass) free(opt_vals.mysql_pass);
    if (opt_vals.mysql_db) free(opt_vals.mysql_db);

    nol_m_auth_cleanup_slaves();

    free_hooks();
}

