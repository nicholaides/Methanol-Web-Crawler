/*-
 * slave-class.c
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include "slave.h"
#include "lmc.h"
#include "master.h"

static slave_t *slave_create(const char *name, uint32_t namelen);
static void slave_zero(slave_t *sl);
static M_CODE slave_add(void *unused, slave_t *sl);
static void slave_destroy(slave_t *sl);
static M_CODE slave_copy(slave_t *sl, slave_t *sl2);
static void* slave_find(void *sl, void *sl2);

/** 
 * defines the slave class to be fed to the LMC parser
 **/
struct lmc_class
nol_slave_class =
{
    .name           = "slave",
    .add_cb         = &slave_add,
    .find_cb        = &slave_find,
    .zero_cb        = &slave_zero,
    .copy_cb        = &slave_copy,
    .constructor_cb = &slave_create,
    .destructor_cb  = &slave_destroy,
    .flags_offs = offsetof(slave_t, flags),
    .opts = {
        /*LMC_OPT_ARRAY("allow", &slave_set_allow),*/
        LMC_OPT_STRING("password", offsetof(slave_t, password)),
    }
};

static slave_t*
slave_create(const char *name, uint32_t namelen)
{
    slave_t *sl;

    if (!(sl = calloc(1, sizeof(slave_t))))
        return 0;

    if (!(sl->name = malloc(namelen+1))) {
        free(sl);
        return 0;
    }

    memcpy(sl->name, name, namelen);
    return sl;
}

M_CODE
slave_add(void *unused, slave_t *sl)
{
    srv.auth.num_slaves += 1;
    if (!(srv.auth.slaves = realloc(srv.auth.slaves,
                    srv.auth.num_slaves*sizeof(slave_t*)))) {
        syslog(LOG_ERR, "out of mem");
        return 0;
    }

    srv.auth.slaves[srv.auth.num_slaves-1] = sl;
    return M_OK;
}

static void
slave_zero(slave_t *sl)
{
    if (sl->password) {
        free(sl->password);
        sl->password = 0;
    }
}

static void
slave_destroy(slave_t *sl)
{
    slave_zero(sl);
    free(sl->name);
    free(sl);
}

static M_CODE
slave_copy(slave_t *sl, slave_t *sl2)
{
    syslog(LOG_ERR, "keywords not supported by slave objects");
    return M_FAILED;
}

static void*
slave_find(void *sl, void *sl2)
{
    syslog(LOG_ERR, "keywords not supported by slave objects");
    return 0;
}

/** 
 * clean up all allocated slave objects
 * in 'srv'
 *
 * called from nol_m_cleanup()
 **/
void
nol_m_auth_cleanup_slaves(void)
{
    int x;
    for (x=0; x<srv.auth.num_slaves; x++) {
        slave_destroy(srv.auth.slaves[x]);
    }
    srv.auth.num_slaves = 0;
    srv.auth.slaves = 0;
}

