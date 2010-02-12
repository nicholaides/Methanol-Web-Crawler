/*-
 * worker.c
 * This file is part of libmetha
 *
 * Copyright (c) 2008, Emil Romanus <emil.romanus@gmail.com>
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

#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <jsapi.h>

#include "str.h"
#include "utable.h"
#include "worker.h"
#include "js.h"

/** 
 * TODO:
 * - When a fatal error occur in a worker, it must have a way of 
 *   terminating all other workers.
 **/

#define inl_ static inline

static M_CODE lm_worker_init(worker_t *w);
static M_CODE lm_worker_init_e4x(worker_t *w);
static M_CODE lm_worker_sort(worker_t *w);
static M_CODE lm_worker_perform(worker_t *w);
static M_CODE lm_worker_call_crawler_init(worker_t *w);
static M_CODE lm_worker_get_robotstxt(worker_t *w, struct host_ent *ent);
static int    lm_worker_wait(worker_t *w);
static M_CODE __lm_worker_default_crawler_init(uehandle_t *h, int argc, const char **argv);
inl_ int lm_worker_bind_url(worker_t *w, url_t *url, filetype_t *ft, int epeek, ulist_t **peek_list);

#ifdef DEBUG
static const char *worker_state_str[] = {
    "WAITING", "RUNNING", "STOPPED",
};
#endif

/** 
 * This is the object class for the 'this' variable in e4x parser callbacks.
 **/
static JSClass worker_jsclass = {
    "worker_t", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/** 
 * Run once without launching a new thread.
 *
 * See lmetha_exec_once() and lmetha_exec_provided().
 **/
M_CODE
lm_worker_run_once(worker_t *w)
{
    if (lm_worker_init(w) != M_OK) {
        LM_ERROR(w->m, "worker initialization failed");
        return M_FAILED;
    }

    if (w->crawler->initial_filetype.ptr) {
        ulist_t *t = lm_utable_top(&w->ue_h->primary);
        t->row[t->sz-1].bind = w->crawler->initial_filetype.ptr->id;
        w->crawler->initial_filetype.ptr = 0;
    } else
        lm_worker_sort(w);

    ue_next(w->ue_h);
    lm_worker_perform(w);
    lm_worker_sort(w);

    return M_OK;
}

/** 
 * Set up the worker so it's ready for execution.
 **/
static M_CODE
lm_worker_init(worker_t *w)
{
    if (lm_worker_init_e4x(w) == M_OK
        && lm_worker_call_crawler_init(w) == M_OK) {

        lm_notify(w->m, LM_EV_THREAD_READY);

        return M_OK;
    }

    return M_ERROR;
}

/** 
 * Call the initial crawlers init function
 **/
static M_CODE
lm_worker_call_crawler_init(worker_t *w)
{
    int x;
    jsval func;
    jsval ret;

    if (w->crawler->init) {
        const char *init_name = w->crawler->init;
#ifdef DEBUG
        fprintf(stderr, "* worker:(%p) calling init function \"%s\"\n",
                w,
                w->crawler->init);
#endif

        if ((init_name = strchr(init_name, '/'))) {
            init_name++;

            JS_BeginRequest(w->e4x_cx);

            if (JS_GetProperty(w->e4x_cx, w->m->e4x_global, init_name, &func) == JS_TRUE 
                && JS_TypeOfValue(w->e4x_cx, func) == JSTYPE_FUNCTION) {
                
                /* create an array to send to the function */
                JSObject *args = JS_NewArrayObject(w->e4x_cx, 0, 0);
                
                for (x=0; x<w->argc; x++) {
                    JSString *tmp = JS_NewStringCopyN(w->e4x_cx, w->argv[x], strlen(w->argv[x]));
                    jsval tmp_v = STRING_TO_JSVAL(tmp);
                    JS_SetElement(w->e4x_cx, args, (jsint)x, &tmp_v);
                }
                jsval a = OBJECT_TO_JSVAL(args);
                JS_CallFunctionValue(w->e4x_cx, w->e4x_this, func, 1, &a, &ret);

                lm_jsval_foreach(w->e4x_cx, ret,
                        (M_CODE (*)(void *, const char *, uint16_t))&ue_add_initial,
                        w->ue_h);

                JS_EndRequest(w->e4x_cx);

                return M_OK;
            } else
                LM_ERROR(w->m, "could not call init function \"%s\"\n",
                                w->crawler->init);

            JS_EndRequest(w->e4x_cx);
        } else {
            LM_ERROR(w->m, "init function for crawler \"%s\" is not a javascript function\n",
                            w->crawler->name);
        }
    }

    __lm_worker_default_crawler_init(w->ue_h, w->argc, w->argv);
    return M_OK;
}
/** 
 * Default crawler init function. This function will build urls
 * from the given arguments and run ue_add_initial() on
 * each.
 **/
static M_CODE
__lm_worker_default_crawler_init(uehandle_t *h, int argc, const char **argv)
{
    int x;
    char *s;
    char *url;

    for (x=0; x<argc; x++) {
        url = s = (char *)argv[x];
        /** 
         * The only thing we need to take care of here is making sure that the 
         * given URL begins with a protocol. The lm_url_set() function, which 
         * will be used for these strings later, does NOT accept URLs wihout 
         * protocol information. Hence, if the given URL here does not begin 
         * with a protocol, we must guess it. If we fail to guess it, we shouldnt
         * even bother trying to add it.
         **/

        while (isalnum(*s))
            s++;
        if (*s != ':') {
            /* this URL does not have a specified protocol, we assume it will be ftp
             * if the URL begins with, for example, "ftp.kernel.org". If it begins with 
             * a slash or a dot, then we assume it is a local URL ("file" protocol).
             * For all other cases, assume HTTP. */
            if (!(s = lm_strtourl(url)))
                return M_OUT_OF_MEM;

            ue_add_initial(h, s, strlen(s));
            free(s);
        } else
            ue_add_initial(h, url, strlen(url));
    }

    return M_OK;
}

/** 
 * This is the main loop of a worker. This function is the entry point
 * after a call to lm_fork_worker()
 **/
static void *
lm_worker_main(void *in)
{
    worker_t      *w      = (worker_t*)in;
    const char    *url;
    crawler_t *new;

    volatile int *num_waiting = &w->m->w_num_waiting;

    pthread_mutex_t  *lk_reply       = &w->m->w_lk_reply;
    pthread_rwlock_t *lk_num_waiting = &w->m->w_lk_num_waiting;

    pthread_mutex_init(&w->lock, 0);
    pthread_cond_init(&w->wakeup_cond, 0);

#ifdef DEBUG
    fprintf(stderr, "* worker:(%p) forked (given %d arguments)\n", w, w->argc);
#endif

    if (lm_worker_init(w) != M_OK) {
        w->m->error_cb(w->m, "worker initialization failed");
        return 0;
    }

    /* sort initial URLs to determine types if initial_filetype is not set */
    if (w->crawler->initial_filetype.ptr) {
        ulist_t *t = lm_utable_top(&w->ue_h->primary);
        int x;
        for (x=0; x<t->sz; x++)
            t->row[x].bind = w->crawler->initial_filetype.ptr->id;
    } else 
        lm_worker_sort(w);

    ue_set_state_info(w->ue_h, w->crawler);

    do {
        if (!(url = ue_next(w->ue_h))) {
            /* out of internal URLs, we'll see if we have any external URLs if external mode is 
             * enabled */
            uehandle_t *h = w->ue_h;

            if (lm_crawler_flag_isset(w->crawler, LM_CRFLAG_EXTERNAL)) {
                struct host_ent *new;
                if ((new = ue_pop_pending(h))) {
                    ue_set_hostent(h, new);
                    h->depth_counter = 0;
                    continue;
                }
            }
            if (lm_worker_wait(w) == LM_WORKER_MSG_CONTINUE)
                continue;
            break;
        }


        if (w->ue_h->primary.sz==2) {
            /* we are about to continue, but first we must see if we 
             * are entering a new domain, and if so we might need to 
             * fetch a robots.txt file depending on the settings */

            /* note that we download robots.txt even if the current crawler 
             * does not care about robots.txt, this is because otherwise we 
             * would have to do locking on the host ent to determine if a
             * robots.txt file has been downloaded already in order to modify
             * it safely. */
            if (w->m->robotstxt) {
                if (!w->ue_h->host_ent->rfetched) {
                    lm_worker_get_robotstxt(w, w->ue_h->host_ent);
                }
            }
            if (lm_crawler_flag_isset(w->crawler, LM_CRFLAG_JAIL)) {
                lm_url_dup(&w->crawler->jail_url, w->ue_h->current);
            }
        }

        if ((new = ue_get_state_info(w->ue_h)) && new != w->crawler)
            lm_worker_set_crawler(w, new);
        else
            ue_set_state_info(w->ue_h, w->crawler);

        /*if (lm_worker_perform(w) == M_OK)*/
        lm_worker_perform(w);
        lm_worker_sort(w);

        /* Check for a message */
        /*
        pthread_mutex_lock(&w->lock);
        */
        switch (w->message) {
            case LM_WORKER_MSG_STOP:
                w->state = LM_WORKER_STATE_STOPPED;
         /*       pthread_mutex_unlock(&w->lock);*/
                goto done;

            case LM_WORKER_MSG_PAUSE:
                /*pthread_mutex_unlock(&w->lock);*/
                lm_worker_wait(w);
                break;

      /*      default:
                pthread_mutex_unlock(&w->lock);*/
        }

        /* Check if another worker is in need of new URLs */
        pthread_rwlock_rdlock(lk_num_waiting);
        if (*num_waiting
                /* we have workers waiting for URLs, but before we can give them 
                 * anything we must verify that we really have any spare URLs to 
                 * provide */
                && lm_utable_top(&w->ue_h->primary)->sz
                /* Attempt to lock the reply-lock, only one worker 
                 * will be able to reply. */
                && pthread_mutex_trylock(lk_reply) == 0) {

            pthread_rwlock_unlock(lk_num_waiting);
            pthread_rwlock_wrlock(lk_num_waiting);

            int x,y,n;
            int per_worker;
            n   = *num_waiting;
            ulist_t *source = lm_utable_top(&w->ue_h->primary);
            if (!lm_crawler_flag_isset(w->crawler, LM_CRFLAG_SPREAD_WORKERS)) {
                if (n < source->sz) {
                    per_worker = source->sz/((*num_waiting)+1);

                    for (x=0; x<n; x++) {
                        worker_t *curr = w->m->waiting_queue[x];
                        pthread_mutex_lock(&curr->lock);
                        if (curr->state == LM_WORKER_STATE_WAITING) {
                            lm_worker_set_crawler(curr, w->crawler);
                            curr->ue_h->depth_counter = w->ue_h->depth_counter;
                            curr->message = LM_WORKER_MSG_CONTINUE;
                            curr->ue_h->host_ent = w->ue_h->host_ent;
                            
                            ulist_t *target = lm_utable_top(&curr->ue_h->primary);
                            if (!target) {
                                lm_utable_inc(&curr->ue_h->primary);
                                target = lm_utable_top(&curr->ue_h->primary);
                            }

                            for (y=(x*per_worker); y<x*(per_worker+1); y++) {
                                url_t *s = lm_ulist_pop(source);
                                if (s) {
                                    url_t *d = lm_ulist_inc(target);
                                    lm_url_swap(s, d);
                                }
                            }
                            pthread_mutex_unlock(&curr->lock);
                            (*num_waiting) --;
                            pthread_cond_signal(&curr->wakeup_cond);
                        } else
                            pthread_mutex_unlock(&curr->lock);
                    }
                }
            } else {
                /* worker spreading is enabled, we will give the pending workers 
                 * external URLs */
                for (x=n-1; x>=0; x--) {
                    worker_t *curr;
                    struct host_ent *host;

                    if (!(host = ue_pop_pending(w->ue_h)))
                        break;
                        
                    curr = w->m->waiting_queue[x];
#ifdef DEBUG
                    fprintf(stderr, "* worker:(%p) giving host entry '%s' to %p\n", w, host->str, curr);
#endif
                    pthread_mutex_lock(&curr->lock);
                    if (curr->state == LM_WORKER_STATE_WAITING) {
                        ue_set_hostent(curr->ue_h, host);
                        curr->ue_h->depth_counter = 0;
                        curr->message = LM_WORKER_MSG_CONTINUE;
                        pthread_mutex_unlock(&curr->lock);
                        (*num_waiting) --;
                        pthread_cond_signal(&curr->wakeup_cond);
                    } else
                        pthread_mutex_unlock(&curr->lock);
                }
            }
            pthread_rwlock_unlock(lk_num_waiting);
            pthread_mutex_unlock(lk_reply);
        } else
            pthread_rwlock_unlock(lk_num_waiting);
    } while (1);

done:
    lm_notify(w->m, LM_EV_THREAD_DESTROY);
    ue_handle_free(w->ue_h);
    pthread_mutex_destroy(&w->lock);
    pthread_cond_destroy(&w->wakeup_cond);
    lm_worker_free(w);
    return 0;
}

/** 
 * Wait for a message from another worker or the 
 * parent metha object. This might be a continue 
 * or a stop message. The message is returned by
 * the function.
 *
 * Note that this function will only add the 
 * worker to the waiting queue, and not remove it
 * once woken up. The external function that
 * wakes this worker up MUST remove the worker
 * from the queue before waking it up.
 **/
static int
lm_worker_wait(worker_t *w)
{
    int msg;
    pthread_mutex_lock(&w->lock);

    if (w->message == LM_WORKER_MSG_NONE) {
        pthread_rwlock_wrlock(&w->m->w_lk_num_waiting);
        w->m->waiting_queue[w->m->w_num_waiting] = w;
        w->m->w_num_waiting ++;
#ifdef DEBUG
        fprintf(stderr, "* worker:(%p) state: WAITING (num_waiting: %d)\n", w, w->m->w_num_waiting);
#endif

        if (w->m->w_num_waiting >= w->m->num_threads)
            lm_notify(w->m, LM_EV_IDLE);

        w->state = LM_WORKER_STATE_WAITING;
        pthread_rwlock_unlock(&w->m->w_lk_num_waiting);

        pthread_cond_wait(&w->wakeup_cond, &w->lock);
    }

    msg = w->message;

    if (msg == LM_WORKER_MSG_STOP)
        w->state = LM_WORKER_STATE_STOPPED;
    else {
        lm_worker_sort(w);
        w->state = LM_WORKER_STATE_RUNNING;
    }

#ifdef DEBUG
    fprintf(stderr, "* worker:(%p) state: %s\n", w, worker_state_str[w->state]);
#endif

    w->message = LM_WORKER_MSG_NONE;
    pthread_mutex_unlock(&w->lock);

    return msg;
}

void
lm_worker_free(worker_t *w)
{
    JS_RemoveRoot(w->e4x_cx, &w->e4x_this);

    JS_BeginRequest(w->e4x_cx);
    JS_GC(w->e4x_cx);
    JS_EndRequest(w->e4x_cx);

    JS_DestroyContext(w->e4x_cx);
    lm_iohandle_destroy(w->io_h);
    lm_attrlist_cleanup(&w->attributes);
}

/** 
 * Switch the current crawler
 **/
M_CODE
lm_worker_set_crawler(worker_t *w, crawler_t *c)
{
    if (c == w->crawler)
        return M_OK;

#ifdef DEBUG
    fprintf(stderr, "* worker:(%p) set crawler to '%s'\n", w, c->name);
#endif

    w->crawler = c;
    /*w->ue_h->depth_counter = 0;*/
    w->ue_h->depth_limit = c->depth_limit;

    return M_OK;
}

/** 
 * Launch a new thread, starting at lm_worker_main()
 **/
M_CODE
lm_fork_worker(metha_t *m, crawler_t *c,
               uehandle_t *ue_h, int argc, 
               const char **argv)
{
    worker_t *w;
    if (!(m->workers = realloc(m->workers, (m->nworkers+1)*sizeof(worker_t*))))
        return M_OUT_OF_MEM;

    if (!(w = calloc(1, sizeof(worker_t))))
        return M_OUT_OF_MEM;

    w->m = m;
    w->ue_h = ue_h;
    w->io_h = lm_iohandle_obtain(&m->io);
    w->argc = argc;
    w->argv = argv;
    w->redirects = 0;
    lm_worker_set_crawler(w, c);

    m->workers[m->nworkers] = w;

    if (pthread_create(&w->thr, 0, &lm_worker_main, (void*)w) != 0)
        return M_FAILED;

    m->nworkers++;

    return M_OK;
}

/** 
 * Discard or sort URLs into filetypes depending 
 * on the current crawler's list of filetypes 
 * and rules.
 *
 * Returns M_OK unless an error occured.
 **/
static M_CODE
lm_worker_sort(worker_t *w)
{
    int        x, epeek;
    ulist_t    *list, *peek_list = 0;
    crawler_t  *cr;
    int        match, lookup;
    int        syn;
    uehandle_t *ue_h = w->ue_h;
    filetype_t *ft;
    url_t      *url;
    const char *mime;
    char       *c;

    /* get the current list of URLs */
    if (!(list = lm_utable_top(&ue_h->primary)))
        return M_FAILED;

    cr     = w->crawler;
    epeek  = (lm_crawler_flag_isset(cr, LM_CRFLAG_EPEEK) && !ue_h->is_peeking)
              ? 1 : 0;
    syn    = w->io_h->io->synchronous;
    lookup = 0;

    for (x=0; x<list->sz; ) {
        url   = lm_ulist_row(list, x);
        match = 0;

        /* first we try to match the URL by looking at the string */
        if ((ft = lm_ftindex_match_by_url(&cr->ftindex, url))) {
            if (ft == LM_FTINDEX_POSSIBLE_MATCH) {
                if (!syn) {
                    if (lm_multipeek_add(w->io_h, url, x) == M_OK) {
                        match = 1;
                        lookup ++;
                    }
                } else {
                    lm_io_head(w->io_h, url);
                    char *mime = w->io_h->transfer.headers.content_type;
                    if (mime) {
                        if ((c = strchr(mime, ';')))
                            *c = '\0';

                        if ((ft = lm_ftindex_match_by_mime(&cr->ftindex, mime))
                                && lm_worker_bind_url(w, url, ft, epeek, &peek_list) == 0)
                            match = 1;
                    }
                }
            } else if (lm_worker_bind_url(w, url, ft, epeek, &peek_list) == 0)
                match = 1;
        }

        if (!match) {
            /* if match is set to 0, we will discard this URL
             * and move the top-most URL to replace its position
             * in the list */
            lm_url_nullify(url);
            lm_url_swap(url, lm_ulist_top(list));
            lm_ulist_dec(list);
        } else
            x++;
    }

    if (!lookup)
        return M_OK;

    ioprivate_t *info;
    CURL        *h;

#ifdef DEBUG
    if (!syn)
        fprintf(stderr, "* worker:(%p) waiting for %d HTTP HEAD requests...\n", w, lookup);
#endif

    while ((info = lm_multipeek_wait(w->io_h))) {
        h   = info->handle;
        url = lm_ulist_row(list, info->identifier);
        match = 0;

        curl_easy_getinfo(h, CURLINFO_CONTENT_TYPE, &mime);

        if (mime) {
            if ((c = strchr(mime, ';')))
                *c = '\0';

            if ((ft = lm_ftindex_match_by_mime(&cr->ftindex, mime))
                    && lm_worker_bind_url(w, url, ft, epeek, &peek_list) == 0)
                match = 1;
        }

        curl_easy_cleanup(h);

        if (!match)
            lm_url_nullify(url);
    }

    /* remove nullified urls from the list */
    x = list->sz-1;
    while (x >= 0 && !(lm_ulist_row(list, x)->sz))
        x--;

    list->sz = x+1;

    for (x=0; x<list->sz; ) {
        if (!(lm_ulist_row(list, x)->sz)) {
            lm_url_swap(lm_ulist_row(list, x), lm_ulist_top(list));
            lm_ulist_dec(list);
        } else
            x++;
    }

    return M_OK;
}

/** 
 * Bind the given URL with the given filetype. Called
 * by lm_worker_sort when a URL matches a filetype.
 *
 * epeek     - 1 or 0 whether epeeking is enabled and allowed
 *             in this case.
 * peek_list - a pointer to a pointer to a ulist_t, this is 
 *             the list of urls where all epeek urls should 
 *             be added. If this list is unset and a url is
 *             to be added to the list, the list should be 
 *             set by this function.
 *
 * Returns 1 if the URL should be nullified
 **/
inl_ int
lm_worker_bind_url(worker_t *w, url_t *url,
                   filetype_t *ft, int epeek,
                   ulist_t **peek_list)
{
    uehandle_t *ue_h = w->ue_h;
    crawler_t  *cr = w->crawler;

    lm_filetype_counter_inc(ft);

    if (FT_FLAG_ISSET(ft, FT_FLAG_HAS_PARSER)
          || FT_FLAG_ISSET(ft, FT_FLAG_HAS_HANDLER)) {
        lm_url_bind(url, ft->id);
        if (LM_URL_ISSET(url, LM_URL_EXTERNAL) && !FT_FLAG_ISSET(ft, FT_FLAG_IGNORE_HOST)) {
            if (epeek) {
                /* add this URL to the epeek list */
                if (!*peek_list) {
                    /* the epeek list is not set up, so this is
                     * probably the first URL that should be 
                     * added to the epeek list, and we must create
                     * an epeek list before we add it */
                    if (lm_utable_inc(&ue_h->primary) != M_OK)
                        return M_OUT_OF_MEM;
                    *peek_list = lm_utable_top(&ue_h->primary);

                    /* now back up our depth counter/limit values
                     * so that we may continue after the epeek
                     * is done */
                    ue_h->depth_counter_bk = ue_h->depth_counter;
                    ue_h->depth_limit_bk = ue_h->depth_limit;
                    ue_h->host_ent_bk = ue_h->host_ent;

                    ue_h->depth_counter = 0;
                    ue_h->depth_limit = cr->peek_limit;
                    ue_h->is_peeking = 1;
                }
                url_t *tmp = lm_ulist_inc(*peek_list);
                /* swap this URL with an empty URL from the new list */
                lm_url_swap(url, tmp);
            } else {
                if (lm_crawler_flag_isset(cr, LM_CRFLAG_EXTERNAL))
                    ue_move_to_secondary(ue_h, url);
            }
        } else 
            return 0;
    } else
        w->m->target_cb(w->m, w, url, 0, ft);

    return 1;
}

/** 
 * Perform a transfer on one URL
 **/
static M_CODE
lm_worker_perform(worker_t *w)
{
    M_CODE r;
    int x;
    jsval ret;
    filetype_t *ft = w->m->filetypes[w->ue_h->current->bind-1];

    if (lm_crawler_flag_isset(w->crawler, LM_CRFLAG_JAIL)) {
        url_t *url = w->ue_h->current;
        url_t *jail_url = &w->crawler->jail_url;
        if (url->file_o-url->host_o-url->host_l
                < jail_url->file_o-jail_url->host_o-jail_url->host_l
                || strncasecmp(jail_url->str+jail_url->host_o+jail_url->host_l,
                               url->str+url->host_o+url->host_l,
                               jail_url->file_o-jail_url->host_o-jail_url->host_l) != 0) {
            return M_OK;
        }
    }

    if (lm_filter_eval_url(&w->ue_h->host_ent->filter, w->ue_h->current) != LM_FILTER_ALLOW)
        return M_OK;

#ifdef DEBUG
    fprintf(stderr, "* worker:(%p) URL: %s\n", w, w->ue_h->current->str);
#endif
    w->m->status_cb(w->m, w, w->ue_h->current);

    if (ft->switch_to.ptr)
        lm_worker_set_crawler(w, ft->switch_to.ptr);
    
    /* prepare the attributes list for this filetype, so
     * that our parsers can fill in values specifically for this
     * url */
    lm_attrlist_prepare(&w->attributes, (const char**)ft->attributes, ft->attr_count);

    /** 
     * call the handler for this filetype, the handler should download
     * the data before we go to the parser chain. If this filetype does
     * not have a handler set, then the default handler for the active
     * crawler is used.
     *
     * TODO: clear all values in 'this' before a javascript handler is
     *       called?
     **/
    wfunction_t *wf = 
        (ft->handler.wf?ft->handler.wf:
            (w->crawler->default_handler.wf?w->crawler->default_handler.wf:0));
    if (wf) {
#ifdef DEBUG
        fprintf(stderr, "* worker:(%p) calling handler '%s'\n", w, wf->name);
#endif
        switch (wf->type) {
            case LM_WFUNCTION_TYPE_NATIVE:
                r = wf->fn.native_handler(w, w->io_h, w->ue_h->current);
                break;
            case LM_WFUNCTION_TYPE_JAVASCRIPT:
                JS_BeginRequest(w->e4x_cx);
                jsval url = STRING_TO_JSVAL(
                        JS_NewStringCopyN(w->e4x_cx, w->ue_h->current->str,
                            w->ue_h->current->sz)
                        );
                r = ((JS_CallFunctionValue(w->e4x_cx, w->e4x_this,
                                           wf->fn.javascript, 1,
                                           &url, &ret)
                        == JS_TRUE) ? M_OK : M_FAILED);
                JS_EndRequest(w->e4x_cx);
            default:
                return M_ERROR;
        }
    } else
        r = lm_io_get(w->io_h, w->ue_h->current);

    if (r != M_OK)
        return r;

    /** 
     * Handler is done, time to do our own logic. If the transfer was over HTTP and we got a 
     * redirect, we will add the URL given by the "Location" header to the URL engine.
     **/
    if (w->io_h->transfer.status_code >= 300
            && w->io_h->transfer.status_code < 400) {
        if (w->io_h->transfer.headers.location) {
#ifdef DEBUG
            fprintf(stderr, "* worker:(%p) %d redirect to '%s'\n",
                    w,
                    w->io_h->transfer.status_code,
                    w->io_h->transfer.headers.location);
#endif
            w->redirects++;
            if (w->redirects >= 20) {
                LM_WARNING(w->m, "breaking out of possible redirect loop");
                w->redirects = 0;
                return M_OK;
            }
            /* add the value of the location header to a 
             * temporary new URL structure, we'll compare it 
             * to the current host name and call ue_move_to_secondary
             * if it is external, otherwise redirect this worker
             * to the URL */
            url_t tmp;
            lm_url_init(&tmp);
            if (lm_url_set(&tmp,
                    w->io_h->transfer.headers.location,
                    strlen(w->io_h->transfer.headers.location)) == M_OK) {
                if (lm_url_hostcmp(&tmp, w->ue_h->current) == 0)
                    ue_revert(w->ue_h, tmp.str, tmp.sz);
                else
                    ue_move_to_secondary(w->ue_h, &tmp);
            }
            lm_url_uninit(&tmp);
            return M_OK;
        }
    }

    w->redirects = 0;

    /**
     * Call each parser in the parser chain, giving the first the
     * data downloaded by the handler. Since a parser might change the 
     * data, we need to make sure the new versions are sent to the 
     * next parser between each. 
     **/
    int last = LM_WFUNCTION_TYPE_NATIVE;
    for (x=0; x<ft->parser_chain.num_parsers; x++) {
        wfunction_t *p = ft->parser_chain.parsers[x];
#ifdef DEBUG
        fprintf(stderr, "* worker:(%p) parser %d of %d: '%s'\n", w, x+1,
                ft->parser_chain.num_parsers,
                p->name);
#endif
        /*TODO: error handling */
        switch (p->type) {
            case LM_WFUNCTION_TYPE_NATIVE:
                if (last == LM_WFUNCTION_TYPE_JAVASCRIPT) {
                    /* convert the data */
                    jsval d;
                    JS_BeginRequest(w->e4x_cx);
                    JS_GetProperty(w->e4x_cx, w->e4x_this, "data", &d);
                    char *from = JS_GetStringBytes(JSVAL_TO_STRING(d));
                    long len  = JS_GetStringLength(JSVAL_TO_STRING(d));
                    if (w->io_h->buf.cap < len) {
                        w->io_h->buf.ptr = realloc(w->io_h->buf.ptr, len);
                        w->io_h->buf.cap = len;
                    }
                    memcpy(w->io_h->buf.ptr, from, len);
                    w->io_h->buf.sz = len;
                    JS_EndRequest(w->e4x_cx);
                }
                p->fn.native_parser(w, &w->io_h->buf, w->ue_h, w->ue_h->current, &w->attributes);
                break;

            case LM_WFUNCTION_TYPE_JAVASCRIPT:
                JS_BeginRequest(w->e4x_cx);
                if (last == LM_WFUNCTION_TYPE_NATIVE) {
                    JSString *tmp;

                    tmp = JS_NewStringCopyN(w->e4x_cx, w->ue_h->current->str, w->ue_h->current->sz);
                    ret = STRING_TO_JSVAL(tmp);
                    JS_SetProperty(w->e4x_cx, w->e4x_this, "url", &ret);

                    tmp = JS_NewStringCopyN(w->e4x_cx, w->io_h->buf.ptr, w->io_h->buf.sz);
                    ret = STRING_TO_JSVAL(tmp);
                    JS_SetProperty(w->e4x_cx, w->e4x_this, "data", &ret);

                    if (w->io_h->transfer.headers.content_type) {
                        tmp = JS_NewStringCopyN(w->e4x_cx, w->io_h->transfer.headers.content_type,
                                                strlen(w->io_h->transfer.headers.content_type));
                        ret = STRING_TO_JSVAL(tmp);
                        JS_SetProperty(w->e4x_cx, w->e4x_this, "content_type", &ret);
                    }

                    ret = INT_TO_JSVAL(w->io_h->transfer.status_code);
                    JS_SetProperty(w->e4x_cx, w->e4x_this, "status_code", &ret);
                }

                if (JS_CallFunctionValue(w->e4x_cx, w->e4x_this,
                                         p->fn.javascript, 0, 0, &ret) != JS_TRUE) {
                    w->m->error_cb(w->m, "calling javascript parser failed");
                } else
                    lm_jsval_foreach(w->e4x_cx, ret,
                            (M_CODE (*)(void *, const char *, uint16_t))&ue_add,
                            w->ue_h);

                JS_EndRequest(w->e4x_cx);
                break;

            default:
                return M_FAILED;
        }

        last = p->type;
    }

    /* parsing is done. if a parser set any of the filetype attributes
     * using lm_attribute_set, then this file is considered a match, 
     * so we send it to the LMOPT_TARGET_FUNCTION callback */
    if (w->attributes.changed)
        w->m->target_cb(w->m, w, w->ue_h->current, &w->attributes, ft);

    return M_OK;
}
/** 
 * This function will set up a SpiderMonkey context derived from the
 * global JSRuntime. It will also initialize the 'this' variable which 
 * can be reached by e4x parser callbacks.
 **/
static M_CODE
lm_worker_init_e4x(worker_t *w)
{
    /** 
     * Set up the spidermonkey context for this worker thread.
     **/
    if (!(w->e4x_cx = JS_NewContext(w->m->e4x_rt, 8192))) {
        LM_ERROR(w->m, "could not create a new JS context");
        return M_FAILED;
    }
#ifdef DEBUG
    fprintf(stderr, "* worker:(%p) created new JS context %p\n", w, w->e4x_cx);
#endif
    JS_BeginRequest(w->e4x_cx);

    JS_SetOptions(w->e4x_cx, JSOPTION_VAROBJFIX | JSOPTION_XML);
    JS_SetVersion(w->e4x_cx, 0);
    JS_SetErrorReporter(w->e4x_cx, &lm_jserror);

    JS_SetGlobalObject(w->e4x_cx, w->m->e4x_global);

    w->e4x_this = JS_NewObject(w->e4x_cx, &worker_jsclass, 0, 0);
    JS_DefineProperty(w->e4x_cx, w->e4x_this, "url", JSVAL_NULL, 0, 0, 0);
    JS_DefineProperty(w->e4x_cx, w->e4x_this, "data", JSVAL_NULL, 0, 0, 0);
    JS_DefineProperty(w->e4x_cx, w->e4x_this, "content_type", JSVAL_NULL, 0, 0, 0);
    JS_DefineProperty(w->e4x_cx, w->e4x_this, "status_code", JSVAL_NULL, 0, 0, 0);
    JS_DefineProperty(w->e4x_cx, w->e4x_this, "protocol", JSVAL_NULL, 0, 0, 0);
    JS_SetPrivate(w->e4x_cx, w->e4x_this, w);

    /** 
     * Construct all the worker objects, this can be custom classes
     * added by a module.
     **/
    int x;
    for (x=0; x<w->m->num_worker_objs; x++) {
        jsval     v;
        JSObject *o;
        if ((o = JS_ConstructObject(w->e4x_cx, w->m->worker_objs[x].class, 0, 0))) {
            v = OBJECT_TO_JSVAL(o);
            JS_DefineProperty(w->e4x_cx, w->e4x_this, w->m->worker_objs[x].name,
                              v, 0, 0, 0);
        }
    }

    JS_SetPrivate(w->e4x_cx, w->e4x_this, w);

    /* set up worker functions */
    if (JS_DefineFunctions(w->e4x_cx, w->e4x_this, lm_js_workerfunctions) == JS_FALSE) {
        LM_ERROR(w->m, "fatal: defining native javascript worker functions failed");
        return M_FAILED;
    }

    JS_AddRoot(w->e4x_cx, &w->e4x_this);
    JS_EndRequest(w->e4x_cx);

    return M_OK;
}

/** 
 * Fetch and parse a robots.txt file for the given domain, 
 * update the host_ent's filter to reflect the rules in 
 * the robots.txt file.
 **/
static M_CODE
lm_worker_get_robotstxt(worker_t *w, struct host_ent *ent)
{
    char *url;
    char *s, *e;
    M_CODE status;
    size_t enable = 1; /* set enable if user agent matches */

    char *opt_s, *opt_e, *val_s, *val_e;

    if (!(url = malloc(ent->len+19))) 
        return M_OUT_OF_MEM;

    url_t u;
    u.sz=sprintf(url, "http://%s/robots.txt", ent->str);
    u.protocol = LM_PROTOCOL_HTTP;
    u.str=url;
    
#ifdef DEBUG
    fprintf(stderr, "* worker:(%p) updating filters (%s)\n", w, url);
#endif

    if ((status = lm_io_get(w->io_h, &u)) == M_OK) {
        for (s=w->io_h->buf.ptr, e=w->io_h->buf.ptr+w->io_h->buf.sz;s<e;s++) {
            while (isspace(*s) && s<e)
                s++; 
            if (*s == '#') {
                do s++; while (s < e && *s != '\n');
                continue;
            }
            opt_s = s;
            if (!(s = opt_e = memchr(s, ':', e-s)))
                break;
            s++;
            while (isspace(*s) && s<e)
                s++; 
            val_s = s;
            if (!(val_e = memchr(s, '\n', e-s)))
                val_e = e;

            int len = opt_e - opt_s;

            if (len == 10 && memcmp(opt_s, "User-agent", 10) == 0) {
                /* temporarily set val_e to \0 */

                if (val_e-val_s == 1 && *val_s == '*') {
                    enable = 1;
                } else {
                    char tmp = *val_e;
                    *val_e = '\0';
                    enable = (size_t)strstr(w->m->io.user_agent, val_s);
                    *val_e = tmp;
                }
            } else if (enable) {
                if (len == 8 && memcmp(opt_s, "Disallow", len) == 0) {
#ifdef DEBUG
                    char tmp = *val_e;
                    *val_e = '\0';
                    fprintf(stderr, "* worker:(%p) Disallow '%s' for '%s'\n", w, val_s, ent->str);
                    *val_e = tmp;
#endif
                    lm_filter_add_rule(&ent->filter, LM_FILTER_DENY,
                                       umex_explicit_strstart(val_s, val_e-val_s));
                } else if (len == 5 && memcmp(opt_s, "Allow", len) == 0) {
#ifdef DEBUG
                    char tmp = *val_e;
                    *val_e = '\0';
                    fprintf(stderr, "* worker:(%p) Allow '%s' for '%s'\n", w, val_s, ent->str);
                    *val_e = tmp;
#endif
                    lm_filter_add_rule(&ent->filter, LM_FILTER_ALLOW,
                                       umex_explicit_strstart(val_s, val_e-val_s));
                }
            }

            s=val_e;
        }
    }

    ent->rfetched = 1;
    free(url);
    return status;
}
