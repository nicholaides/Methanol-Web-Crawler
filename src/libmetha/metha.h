/*-
 * metha.h
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

#ifndef _METHA__H_
#define _METHA__H_

#include <stdarg.h>
#include <pthread.h>
#include <jsapi.h>

#include "filetype.h"
#include "attr.h" 
#include "crawler.h"
#include "errors.h"
#include "io.h"
#include "url.h"
#include "wfunction.h"
#include "events.h"
#include "lmopt.h"
#include "../libmethaconfig/lmc.h"

enum {
    LM_SIGNAL_EXIT,
    LM_SIGNAL_STOP,
    LM_SIGNAL_PAUSE,
    LM_SIGNAL_CONTINUE,
};

enum {
    LM_STATE_INIT = 0,
    LM_STATE_PREPARED,
    LM_STATE_RUNNING,
};

struct observer_pool {
    unsigned int     count;
    struct observer *o;
};

struct observer {
    void *arg2;
    M_CODE (*callback)(void *, void *);
};

struct worker_object {
    const char *name;
    JSClass    *class;
};

struct script_desc {
    char *full;
    char *name;
    JSScript   *script;
};

typedef struct metha {
    io_t          io;
    ue_t          ue;

    /* only used if lmetha_exec_async() is called */
    pthread_t       thr;

    /* configuration file parser from libmethaconfig */
    lmc_parser_t *lmc;

    /* pipe used for signaling */
    int msg_fd[2];

    /* lmetha_exec_once() and lmetha_exec_provided() will save 
     * their results here, so we can pick it up the next time
     * any exec function is called */
    uehandle_t *ueh_save;

    /* worker (threads) locks */
    int w_message_id;
    struct {crawler_t *cr; uehandle_t *ue_h;} w_message_ret;

    pthread_mutex_t  w_lk_reply;
    pthread_rwlock_t w_lk_num_waiting;
    volatile int     w_num_waiting;
    struct worker  **waiting_queue;

    int crawler; /* initial crawler */

    filetype_t     **filetypes;
    crawler_t      **crawlers;
    int              num_filetypes;
    int              num_crawlers;

    char           **configs;
    int              num_configs;

    wfunction_t    **functions;
    int              num_functions;

    struct worker   **workers;
    int               nworkers;

    struct lm_mod  **modules;
    int              num_modules;

    struct script_desc *scripts;
    int                 num_scripts;

    JSRuntime        *e4x_rt;
    JSContext        *e4x_cx;
    JSObject         *e4x_global;

    struct lm_scope  *scopes;
    int               num_scopes;

    /* primary and secondary script dir */
    char       *script_dir1;
    char       *script_dir2;

    char       *conf_dir1;
    char       *conf_dir2;

    char       *module_dir;

    struct observer_pool  observer_pool[LM_EV_COUNT];
    struct worker_object *worker_objs;
    int                   num_worker_objs;

    void   (*status_cb)(struct metha *, struct worker *, url_t *);
    void   (*target_cb)(struct metha *, struct worker *, url_t *, attr_list_t *, filetype_t *);
    void   (*error_cb)(struct metha *, const char *s, ...);
    void   (*warning_cb)(struct metha *, const char *s, ...);
    void   (*event_cb)(struct metha *, unsigned ev);

    int builtin_parsers;
    int num_threads;

    int robotstxt; /* is robots.txt support enabled in ANY crawler? */
    int state;
} metha_t;

/* metha.c */
M_CODE   lmetha_global_init(void);
void     lmetha_global_cleanup(void);

metha_t *lmetha_create(void);
M_CODE   lmetha_setopt(metha_t *m, LMOPT opt, ...);
M_CODE   lmetha_prepare(metha_t *m);
void     lmetha_destroy(metha_t *m);

M_CODE   lmetha_exec(metha_t *m, int argc, const char **argv); 
M_CODE   lmetha_exec_provided(metha_t *m, const char *base_url, const char *buf, size_t len);
M_CODE   lmetha_exec_async(metha_t *m, int argc, const char **argv);

M_CODE   lmetha_add_wfunction(metha_t *m, const char *name, uint8_t type, uint8_t purpose, ...);
M_CODE   lmetha_add_filetype(metha_t *m, filetype_t *ft);
M_CODE   lmetha_add_crawler(metha_t *m, crawler_t *cr);
M_CODE   lmetha_register_worker_object(metha_t *m, const char *name, JSClass *class);
M_CODE   lmetha_register_jsfunction(metha_t *m, const char *name, JSNative fun, unsigned argc);
M_CODE   lmetha_init_jsclass(metha_t *m, JSClass *class, JSNative constructor, uintN nargs, JSPropertySpec *ps, JSFunctionSpec *fs, JSPropertySpec *static_ps, JSFunctionSpec *static_fs);
M_CODE   lmetha_signal(metha_t *m, int sig);
M_CODE   lmetha_start(metha_t *m);
M_CODE   lmetha_load_config(metha_t *m, const char *file);
M_CODE   lmetha_reset(metha_t *m);
M_CODE   lmetha_read_config(metha_t *m, const char *buf, int size);
filetype_t* lmetha_get_filetype(metha_t *m, const char *name);
crawler_t*  lmetha_get_crawler(metha_t *m, const char *name);

/* io.c */
M_CODE lm_iothr_launch(io_t *io);
M_CODE lm_init_io(io_t *io, metha_t *m);
void   lm_uninit_io(io_t *io);
M_CODE lm_get(struct crawler *c, url_t *url, iostat_t *stat);
M_CODE lm_peek(struct crawler *c, url_t *url, iostat_t *stat);
M_CODE lm_multipeek_add(iohandle_t *ioh, url_t *url, int id);
ioprivate_t* lm_multipeek_wait(iohandle_t *ioh);

/* errors.c */
void lm_default_status_reporter(metha_t *, struct worker *, url_t *);
void lm_default_target_reporter(metha_t *, struct worker *, url_t *, attr_list_t *, filetype_t *);
void lm_default_error_reporter(metha_t *, const char *s, ...);
void lm_default_warning_reporter(metha_t *, const char *s, ...);

/* events.c */
void lm_default_event_handler(metha_t *m, unsigned ev);

#endif

