/*-
 * worker.h
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

#ifndef _WORKER__H_
#define _WORKER__H_

#include "metha.h"
#include "crawler.h"
#include "attr.h"
#include "filetype.h"
#include "urlengine.h"
#include "io.h"
#include <jsapi.h>

#ifdef inl_
#undef inl_
#endif

#define inl_ static inline

enum {
    LM_WORKER_MSG_NONE,
    LM_WORKER_MSG_STOP,
    LM_WORKER_MSG_PAUSE,
    LM_WORKER_MSG_CONTINUE,
};

enum {
    LM_WORKER_STATE_WAITING,
    LM_WORKER_STATE_RUNNING,
    LM_WORKER_STATE_STOPPED,
};

/* used when workers share URL information */
typedef struct worker_info {
    crawler_t  *cr;
    uehandle_t *ue_h;
    int         depth;
} worker_info_t;

typedef struct worker {
    metha_t      *m;
    crawler_t    *crawler;
    uehandle_t   *ue_h;
    iohandle_t   *io_h;

    JSContext    *e4x_cx;
    JSObject     *e4x_this;

    int          argc;
    const char **argv;

    int          state;
    int          message;
    int          redirects;

    /* attribute list used for all urls matching a filetype */
    attr_list_t attributes;

    pthread_t       thr;
    pthread_mutex_t lock; /* lock access to 'state' and 'message' */
    pthread_cond_t  wakeup_cond;
} worker_t;

M_CODE lm_fork_worker(metha_t *m, crawler_t *c, uehandle_t *ue_h, int argc, const char **argv);
void   lm_worker_free(worker_t *w);
M_CODE lm_worker_run_once(worker_t *w);
M_CODE lm_worker_set_crawler(worker_t *w, crawler_t *c);

#endif
