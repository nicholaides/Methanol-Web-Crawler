/*-
 * events.c
 * This file is part of libmetha
 *
 * Copyright (C) 2008, Emil Romanus <emil.romanus@gmail.com>
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

#include "metha.h"
#include "events.h"

/** 
 * Notify all observers of the given event
 **/
M_CODE
lm_notify(metha_t *m, unsigned ev)
{
    int x;

    m->event_cb(m, ev);

    if (ev < LM_EV_COUNT)
        for (x=0; x<m->observer_pool[ev].count; x++)
            m->observer_pool[ev].o[x].callback(m, m->observer_pool[ev].o[x].arg2);

    return M_FAILED;
}

/** 
 * Attach an observer callback to the given event. 'private' will
 * be sent as the second argument to the callback when it is invoked,
 * 'm' will always be sent as the first argument to the callback.
 **/
M_CODE
lmetha_attach_observer(struct metha *m, int type,
                       M_CODE (*callback)(void *, void *), void *private)
{
    if (type >= LM_EV_COUNT)
        return M_FAILED;

    struct observer_pool *pool = &m->observer_pool[type];

    if (!(pool->o = realloc(pool->o, (pool->count+1)*sizeof(struct observer))))
        return M_OUT_OF_MEM;

    pool->o[pool->count].arg2     = private;
    pool->o[pool->count].callback = callback;
    pool->count ++;

    return M_OK;
}

/** 
 *
 **/
M_CODE
lmetha_detach_observer(struct metha *m, int type,
                       M_CODE (*callback)(void *, void *))
{
    return M_FAILED;
}

/** 
 * Default event handler. May be overridden using LMOPT_EV_FUNCTION.
 **/
void
lm_default_event_handler(metha_t *m, unsigned ev)
{
    switch (ev) {
        case LM_EV_IDLE:
            lmetha_signal(m, LM_SIGNAL_EXIT);
            break;
    }
}

