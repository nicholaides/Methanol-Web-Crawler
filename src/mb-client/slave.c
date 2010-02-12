/*-
 * slave.c
 * This file is part of mb-client
 *
 * Copyright (c) 2009, Emil Romanus <emil.romanus@gmail.com>
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
 * http://metha-sys.org/
 */

#include <string.h>

#include "client.h"
#include "nolp.h"

static int mbc_slave_on_start(nolp_t *no, char *buf, int size);
static int mbc_slave_on_stop(nolp_t *no, char *buf, int size);
static int mbc_slave_on_continue(nolp_t *no, char *buf, int size);
static int mbc_slave_on_pause(nolp_t *no, char *buf, int size);
static int mbc_slave_on_exit(nolp_t *no, char *buf, int size);
static int mbc_slave_on_config(nolp_t *no, char *buf, int size);
static int mbc_slave_on_config_recv(nolp_t *no, char *buf, int size);

/* commands received from the slave, to this client */
struct nolp_fn sl_commands[] = {
    {"START", &mbc_slave_on_start},
    {"STOP", &mbc_slave_on_stop},
    {"CONTINUE", &mbc_slave_on_continue},
    {"PAUSE", &mbc_slave_on_pause},
    {"EXIT", &mbc_slave_on_exit},
    {"CONFIG", &mbc_slave_on_config},
    {0}
};

char *arg;

static int
mbc_slave_on_start(nolp_t *no, char *buf, int size)
{
    char *p;
    buf[size] = '\0';
    if (!(p = strchr(buf, ' '))) {
        print_error("%s", "weird START format from slave");
        return -1;
    }
    *p = '\0';
    p++;

    print_debug("received url '%s' from slave", p);

    if (mbc.state == MBC_STATE_RUNNING) {
        /* we received a new START signal even though the slave
         * should have known we're already running */
        print_debug("%s", "START override, why? User signal?");
        lmetha_signal(mbc.m, LM_SIGNAL_EXIT);
        mbc_end_session();
    }

    lmetha_reset(mbc.m);
    if (lmetha_setopt(mbc.m, LMOPT_INITIAL_CRAWLER, buf) != M_OK) {
        print_error("unknown crawler '%s' from slave", buf);
        return -1;
    }
    if (arg)
        free(arg);
    if (!(arg = strdup(p)))
        return -1;

    send(mbc.sock, "STATUS 1\n", 9, 0);
    lmetha_exec_async(mbc.m, 1, &arg);

    mbc.state = MBC_STATE_RUNNING;
    return 0;
}

static int
mbc_slave_on_stop(nolp_t *no, char *buf, int size)
{
    return 0;
}

static int
mbc_slave_on_continue(nolp_t *no, char *buf, int size)
{
    lmetha_signal(mbc.m, LM_SIGNAL_CONTINUE);
    return 0;
}

static int
mbc_slave_on_pause(nolp_t *no, char *buf, int size)
{
    lmetha_signal(mbc.m, LM_SIGNAL_PAUSE);
    return 0;
}

static int
mbc_slave_on_exit(nolp_t *no, char *buf, int size)
{
    lmetha_signal(mbc.m, LM_SIGNAL_EXIT);
    return 0;
}

static int
mbc_slave_on_config(nolp_t *no, char *buf, int size)
{
    nolp_expect(mbc.no, atoi(buf), &mbc_slave_on_config_recv);
    return 0;
}

static int
mbc_slave_on_config_recv(nolp_t *no, char *buf, int size)
{
    static int config_read = 0;
    M_CODE r;

    if (!config_read) {
        if ((r = lmetha_read_config(mbc.m, buf, size)) != M_OK) {
            print_error("reading libmetha config failed: %s", lm_strerror(r));
            return -1;
        }
        if ((r = lmetha_prepare(mbc.m)) != M_OK) {
            print_error("preparing libmetha object failed: %s", lm_strerror(r));
            return -1;
        }
    } else {
        print_warning("%s", "can not reload configuration");
    }

    /* notify the slave that we're idle, this will hopefully
     * make the slave send us a START command with URL and
     * crawler info */
    send(mbc.sock, "STATUS 0\n", 9, 0);

    config_read = 1;
    return 0;
}
