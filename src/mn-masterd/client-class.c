/*-
 * client-class.c
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
#include "client.h"
#include "lmc.h"
#include "master.h"

static client_t *client_create(const char *name, uint32_t namelen);
static void client_zero(client_t *cl);
static M_CODE client_add(void *unused, client_t *cl);
static void client_destroy(client_t *cl);
static M_CODE client_copy(client_t *cl, client_t *cl2);
static void* client_find(void *cl, void *cl2);

/** 
 * defines the client class to be fed to the LMC parser
 **/
struct lmc_class
nol_client_class =
{
    .name           = "client",
    .add_cb         = &client_add,
    .find_cb        = &client_find,
    .zero_cb        = &client_zero,
    .copy_cb        = &client_copy,
    .constructor_cb = &client_create,
    .destructor_cb  = &client_destroy,
    .flags_offs = offsetof(client_t, flags),
    .opts = {
        /*LMC_OPT_ARRAY("allow", &client_set_allow),*/
        LMC_OPT_STRING("password", offsetof(client_t, password)),
        LMC_OPT_STRING("slave", offsetof(client_t, slave)),
    }
};

static client_t*
client_create(const char *name, uint32_t namelen)
{
    client_t *cl;

    if (!(cl = calloc(1, sizeof(client_t))))
        return 0;

    if (!(cl->name = malloc(namelen+1))) {
        free(cl);
        return 0;
    }

    memcpy(cl->name, name, namelen);
    return cl;
}

M_CODE
client_add(void *unused, client_t *cl)
{
    srv.auth.num_clients += 1;
    if (!(srv.auth.clients = realloc(srv.auth.clients,
                    srv.auth.num_clients*sizeof(client_t*)))) {
        syslog(LOG_ERR, "out of mem");
        return 0;
    }

    srv.auth.clients[srv.auth.num_clients-1] = cl;
    return M_OK;
}

static void
client_zero(client_t *cl)
{
    if (cl->password) {
        free(cl->password);
        cl->password = 0;
    }
}

static void
client_destroy(client_t *cl)
{
    client_zero(cl);
    free(cl->name);
    free(cl);
}

static M_CODE
client_copy(client_t *cl, client_t *cl2)
{
    syslog(LOG_ERR, "keywords not supported by client objects");
    return M_FAILED;
}

static void*
client_find(void *cl, void *cl2)
{
    syslog(LOG_ERR, "keywords not supported by client objects");
    return 0;
}

/** 
 * clean up all allocated client objects
 * in 'srv'
 *
 * called from nol_m_cleanup()
 **/
void
nol_m_auth_cleanup_clients(void)
{
    int x;
    for (x=0; x<srv.auth.num_clients; x++) {
        client_destroy(srv.auth.clients[x]);
    }
    srv.auth.num_clients = 0;
    srv.auth.clients = 0;
}

