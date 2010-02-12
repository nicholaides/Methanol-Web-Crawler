/*-
 * urlengine.c
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

#include <jsapi.h>
#include <ctype.h>
#include <string.h>
#include "urlengine.h"

/*#define UE_DEBUG*/
#ifdef _DEBUG
#undef _DEBUG
#endif

#ifdef UE_DEBUG
#define _DEBUG(x, ...) fprintf(stderr, x, __VA_ARGS__);
#else
#define _DEBUG(x, ...)
#endif

static struct host_ent *ue_hostent_create(uehandle_t *h, const char *str, uint16_t len, int secondary_n, int add_pending);
static void ue_hostent_free(struct host_ent *p);
static M_CODE ue_push_pending(uehandle_t *h, struct host_ent *p);
/*static M_CODE ue_remove_pending(uehandle_t *h, struct host_ent *p);*/
struct host_ent *ue_get_hostent(uehandle_t *h, const char *host, uint16_t host_sz, int add_pending);

M_CODE
ue_init(ue_t *ue)
{
    int x;

    for (x=0; x<UE_SECONDARY_SIZE; x++)
        if (pthread_rwlock_init(&ue->secondary[x].rwlock, 0) != 0)
            return M_FAILED;

    if (pthread_mutex_init(&ue->pending_lk, 0) != 0)
        return M_FAILED;

    if (!(ue->pending.st = malloc(8*sizeof(struct host_ent *))))
        return M_OUT_OF_MEM;

    ue->pending.sz = 0;
    ue->pending.cap = 8;


    return M_OK;
}

void
ue_uninit(ue_t *ue)
{
    int x, y;
    struct host_ent *curr, *next;

    for (x=0; x<UE_SECONDARY_SIZE; x++) {
        pthread_rwlock_destroy(&ue->secondary[x].rwlock);

        /* clean up the host entries */
        for (y=0; y<8; y++) {
            if ((curr = ue->secondary[x].hosts[y])) {
                do {
                    next = curr->next;
                    ue_hostent_free(curr);
                } while ((curr = next));
            }
        }
    }

    pthread_mutex_destroy(&ue->pending_lk);

    /*
    for (x=0; x<ue->pending.sz; x++)
        ue_hostent_free(ue->pending.st[x]);
        */
    free(ue->pending.st);
}

/** 
 * Each thread (worker) must obtain its own url engine handle
 **/
uehandle_t*
ue_handle_obtain(ue_t *ue)
{ 
    uehandle_t *ret = calloc(1, sizeof(uehandle_t));

    ret->parent = ue;
    ret->depth_limit = 1;

    lm_utable_init(&ret->primary);
    return ret;
}

void
ue_handle_free(uehandle_t *h)
{
    lm_utable_uninit(&h->primary);
    free(h);
}

/** 
 * Used when adding URLs initially before a session has begun
 **/
M_CODE
ue_add_initial(uehandle_t *h, const char *url, uint16_t len)
{
    url_t   *t;
    ulist_t *list;

    if (!(list = lm_utable_top(&h->primary))) {
        /* try to increase the utable */
        if (lm_utable_inc(&h->primary) != M_OK)
            return M_FAILED;
        if (!(list = lm_utable_top(&h->primary)))
            return M_FAILED;
    }

    if (!(t = lm_ulist_inc(list)))
        return M_FAILED;

    if (lm_url_set(t, url, len) != M_OK)
        return M_FAILED;

    /* pretty slow way of adding the URL to its host's cache, but
     * it works ok right now */
    ue_set_host(h, t->str+t->host_o, t->host_l);
    pthread_mutex_lock(&h->host_ent->lock);
    if (!mtrie_tryadd(&h->host_ent->cache, t)) {
        pthread_mutex_unlock(&h->host_ent->lock);
        list->sz--;
        return M_FAILED;
    }
    pthread_mutex_unlock(&h->host_ent->lock);
    return M_OK;
}

/** 
 * Revert the last depth-increase and add the given URL
 * to the top of the last table. Used when Location:
 * headers are sent in HTTP.
 **/
M_CODE
ue_revert(uehandle_t *h, const char *url, uint16_t len)
{
    h->depth_counter --;
    lm_utable_dec(&h->primary);

    return ue_add(h, url, len);
}

/** 
 * Add a relative or absolute URL.
 **/
M_CODE
ue_add(uehandle_t *h, const char *url, uint16_t len)
{
    url_t   *t;
    ulist_t *list;
    int x;
    M_CODE ret;

    if (!(list = lm_utable_top(&h->primary)))
        return M_FAILED;
    if (!(t = lm_ulist_inc(list)))
        return M_FAILED;

    /* now we have the list in 'list' and a place for the
     * URL in 't' */

    /* fix the given url depending on its syntax */
    if (*url == '/') {
        /* the URL starts with a '/' and should thus presumably be appended
         * to the current host */
        if ((ret = lm_url_combine(t, h->current, url, len)) != M_OK)
            goto failed;
        goto cache_check;
    }
    for (x=0; x<len; x++) {
        if (!isalnum(url[x])) {
            if (url[x] == ':') {
                if (lm_url_set(t, url, len) == M_OK) {
                    /* now we need to check whether the URL is external or not,
                     * by comparing the host of the URL with the current URL's host */
                    if (t->protocol != h->current->protocol
                        || lm_url_hostcmp(t, h->current) != 0)
                        t->flags |= LM_URL_EXTERNAL; /* set the "external" flag for this URL if
                                                        the host doesn't match the current URL's */
                    goto cache_check;
                } else
                    goto failed;
            }
            break;
        }
    }
    /* the url is something like "hello/dsa" or "xyz.html", merge it with current host and path */
    if ((ret = lm_url_combine(t, h->current, url, len)) != M_OK)
        goto failed;

cache_check:
    /* if we can't add this to the cache, it's probably already crawled or added to the list. 
     * and if so, we should remoev it from the list again and thus discard it */

    if (t->flags & LM_URL_EXTERNAL) {
        /* url is external, we can't use the current cache since that
         * cache is for the current host name only. We must find a matching
         * cache for the given host */
        uint16_t len = t->host_l-(LM_URL_ISSET(t, LM_URL_WWW_PREFIX)?4:0);
        uint16_t o   = t->host_o+(LM_URL_ISSET(t, LM_URL_WWW_PREFIX)?4:0);
        struct host_ent *ent = ue_get_hostent(h, t->str+o, len, 1);

        pthread_mutex_lock(&ent->lock);
        if (!mtrie_tryadd(&ent->cache, t)) {
            pthread_mutex_unlock(&ent->lock);
            list->sz--;
            return M_FAILED;
        }
        pthread_mutex_unlock(&ent->lock);
    } else {
        /* lock the current cache and add the URL */
        pthread_mutex_lock(&h->host_ent->lock);
        if (!mtrie_tryadd(&h->host_ent->cache, t)) {
            pthread_mutex_unlock(&h->host_ent->lock);
            list->sz--;
            return M_FAILED;
        }
        pthread_mutex_unlock(&h->host_ent->lock);
    }

    return M_OK;

failed:
    list->sz--;
    return M_FAILED;
}


/**
 * Used to find or create a host entry for a given host 
 * name. The uehandle will save a pointer to the host entry
 * and set its active URL cache to the host entry's.
 **/
M_CODE
ue_set_host(uehandle_t *h, const char *host, uint16_t host_sz)
{
    if (host_sz > 4 && strncasecmp(host, "www.", 4) == 0) {
        host+=4;
        host_sz-=4;
    }

    h->host_ent = ue_get_hostent(h, host, host_sz, 0);

    if (!h->host_ent) {
        /*lm_error("internal: find/create host entry '%s' failed, file a bug report\n", host);*/
        abort();
    }

    return M_OK;
}

/** 
 * Called by ue_set_host() and ue_move_to_secondary()
 *
 * If a matching host entry does not exists, it will be
 * created. So this function *should* always return a 
 * valid pointer unless something goes really wrong.
 **/
struct host_ent *
ue_get_hostent(uehandle_t *h, const char *host, uint16_t host_sz, int add_pending)
{
    struct host_ent *p, *last = 0;
    uint16_t n = host_sz;
    ue_t *ue = h->parent;
    pthread_rwlock_t *rwl;

    n = ((((n << 1) ^ *host) << 1) ^ *(host+host_sz-1)) & 0x3f;
    p = ue->secondary[n].hosts[host_sz & 7];
    rwl = &ue->secondary[n].rwlock;

    /** 
     * Lock the secondary table for reading, so we can find a matching
     * host entry. If the host entry is not found, we will re-lock
     * the n'th entry in the secondary table again, but for writing
     * that time.
     **/
    pthread_rwlock_rdlock(rwl);

    while (p) {
        if (p->len == host_sz && strncasecmp(host, p->str, host_sz) == 0)
            break;
        last = p;
        p = p->next;
    }

    if (!p) {
        /* no matching host entry was found, we must add one to last->next */

        /* relock the table */
        pthread_rwlock_unlock(rwl);
        pthread_rwlock_wrlock(rwl);
        struct host_ent **np;

        /* there is a SLIGHT possibility that another thread 
         * added the host between the unlock and wrlock call (possibly,
         * another worker could have locked rwl for writing while we had it locked 
         * for reading), thus we must check again if the host entry still does 
         * not exists */
        if (!*(np = (last?&last->next:&ue->secondary[n].hosts[host_sz & 7]))) {
            /* still didnt exists, so we create it */
            if (!(p = ue_hostent_create(h, host, host_sz, n, add_pending))) {
                pthread_rwlock_unlock(rwl);
                return 0;
            }

            *np = p;
        } else
            p = *np;
    }

    pthread_rwlock_unlock(rwl);

    return p;
}



/** 
 * Set the current host using a host_ent
 *
 * This function will be used when workers share URLs.
**/
M_CODE
ue_set_hostent(uehandle_t *h, struct host_ent *ent)
{
    h->host_ent = ent;

    ulist_t *top;
    if (lm_utable_inc(&h->primary) == M_OK
        && (top = lm_utable_top(&h->primary))) {
        int x;
        for (x=0; x<ent->list.sz; x++) {
            url_t *t = lm_ulist_inc(top);
            lm_url_swap(t, &ent->list.row[x]);
        }

        lm_ulist_uninit(&ent->list);
    } else {
        /*lm_error("increasing primary utable failed, file a bug report\n");*/
        abort();
    }

    return M_OK;
}


/** 
 * Pick a next target URL. Return a pointer to the string, and
 * set the current url of the handle to a pointer to the 
 * url_t.
 *
 * This function will set up a ulist where all urls sent to
 * ue_add will be added.
 **/
const char*
ue_next(uehandle_t *h)
{
    ulist_t *top;
    url_t   *url;

    if (h->depth_limit) {
        while (h->depth_counter >= h->depth_limit) {
            lm_utable_dec(&h->primary);
            h->depth_counter --;
        }
    }

    if (!(top = lm_utable_top(&h->primary)))
        return 0;
    while (!(url = lm_ulist_pop(top))) {
        /* popping a URL from the current list failed...
         * we'll try to decrease the utable size to 
         * get the next (or actually the previous) list */
        if (lm_utable_dec(&h->primary) != M_OK || !(top = lm_utable_top(&h->primary))) {
            return 0;
        }

        if (h->depth_counter)
            h->depth_counter --;

        if (!h->depth_counter && h->is_peeking) {
            /* reset counter if we are in an external peek */
            h->depth_counter = h->depth_counter_bk;
            h->depth_limit   = h->depth_limit_bk;
            h->is_peeking    = 0;
            ue_set_host(h, h->host_ent_bk->str, h->host_ent_bk->len);

            if (h->depth_counter >= h->depth_limit) {
                if (lm_utable_dec(&h->primary) != M_OK || !(top = lm_utable_top(&h->primary))) {
                    return 0;
                }
            }
        }
    }

    h->state_info = lm_utable_top(&h->primary)->private;
    /* increase the size of the utable so that our found URLs
     * will be put in the next list and not the current */
    lm_utable_inc(&h->primary);
    h->depth_counter ++;

    h->current = url;

    /* special case for the root list, we accept multiple host names here
     * since the user should be able to add different hosts as initial urls
     * */
    if (!h->depth_counter)
        ue_set_host(h, h->current->str+h->current->host_o, h->current->host_l);

    return url->str;
}

void *
ue_get_state_info(uehandle_t *h)
{
    return h->state_info;
}

void
ue_set_state_info(uehandle_t *h, void *info)
{
    ulist_t *top;
    if (!(top = lm_utable_top(&h->primary)))
        return;

    top->private = info;
}

/** 
 * Start an epeek, back up depth limit/counter
 * values so that we can continue when the epeek 
 * is stopped.
 **/
M_CODE
ue_start_epeek(uehandle_t *h)
{
    return M_OK;
}

/** 
 * Stop the epeek, restore backed up values.
 **/
M_CODE
ue_stop_epeek(uehandle_t *h)
{
    return M_OK;
}

/** 
 * Move the given URL to a secondary cache. A secondary
 * cache is a cache that does not apply to the current 
 * host name of the URL being crawled.
 **/
M_CODE
ue_move_to_secondary(uehandle_t *h, url_t *url)
{
    struct host_ent *p;
    uint16_t host_o;
    uint16_t host_l;
    char     *host;

    if (LM_URL_ISSET(url, LM_URL_WWW_PREFIX)) {
        host_o = url->host_o+4;
        host_l = url->host_l-4;
    } else {
        host_o = url->host_o;
        host_l = url->host_l;
    }

    host = url->str+host_o;

    p = ue_get_hostent(h, host, host_l, 1);

    pthread_mutex_lock(&p->lock);
    url_t *t = lm_ulist_inc(&p->list);
    lm_url_swap(t, url);
    pthread_mutex_unlock(&p->lock);

    return M_OK;
}

/**
 * Create a host name entry.
 *
 * If 'add_pending' is set to 1, the host will be added to the
 * url engine's pending stack.
 *
 * secondary_n should be the position in ue_t->secondary in which this
 * host entry is going to be added.
 **/
static struct host_ent *
ue_hostent_create(uehandle_t *h, const char *str,
                  uint16_t len, int secondary_n, int add_pending)
{
    struct host_ent *p;

    if (!(p = calloc(1, sizeof(struct host_ent))))
        return 0;
    if (!(p->str = malloc(len+1))) {
        free(p);
        return 0;
    }

    pthread_mutex_init(&p->lock, 0);

    memcpy(p->str, str, len);
    p->str[len] = '\0';
    p->len = len;
    p->secondary_n = secondary_n;

#ifdef DEBUG
    fprintf(stderr, "* uehandle:(%p) created new host entry for '%s'\n", h, p->str);
#endif

    lm_ulist_init(&p->list, 2);

    if (add_pending) /* Add it to the pending list. Or else no worker will be able to find it. */
        ue_push_pending(h, p);
    else {
        p->pending = 0;
        p->pending_pos = 0;
    }

    return p;
}

static void
ue_hostent_free(struct host_ent *p)
{
    pthread_mutex_destroy(&p->lock);
    mtrie_cleanup(&p->cache);
    lm_ulist_uninit(&p->list);
    free(p->str);
    free(p);
}

/** 
 * Add a host entry to the url engine's pending stack, if pos
 * is not 0, set the uint pointed to by it to which position
 * in the stack this host entry was added to.
 **/
static M_CODE
ue_push_pending(uehandle_t *h, struct host_ent *p)
{
    struct lm_pending_e_st *st = &h->parent->pending;
    pthread_mutex_lock(&h->parent->pending_lk);

    if (st->sz >= st->cap) {
        st->cap *= 2;
        if (!(st->st = realloc(st->st, st->cap*sizeof(struct host_ent*)))) {
            pthread_mutex_unlock(&h->parent->pending_lk);
            return M_OUT_OF_MEM;
        }
    }

    p->pending     = 1;
    p->pending_pos = st->sz;
    st->st[st->sz] = p;
    st->sz ++;

    pthread_mutex_unlock(&h->parent->pending_lk);
    return M_OK;
}

/** 
 * Remove the given host entry from the pending stack. This does not 
 * affect the host entry itself other than modifying its pending and
 * pending_pos values.
 **/
/*
static M_CODE
ue_remove_pending(uehandle_t *h, struct host_ent *p)
{
    struct lm_pending_e_st *st = &h->parent->pending;

    pthread_mutex_lock(&h->parent->pending_lk);

    if (p->pending) {
        if (st->sz > 1) {
            st->st[p->pending_pos] = st->st[st->sz-1];
            st->st[p->pending_pos]->pending_pos = p->pending_pos;
        } else {
            st->st[0] = 0;
        }
        p->pending = 0;
        p->pending_pos = 0;
        st->sz --;
    }

    pthread_mutex_unlock(&h->parent->pending_lk);

    return M_OK;
}
*/

/** 
 * Return a pointer to the top pneding stack entry
 **/
struct host_ent*
ue_pop_pending(uehandle_t *h)
{
    struct lm_pending_e_st *st = &h->parent->pending;
    pthread_mutex_lock(&h->parent->pending_lk);

    if (st->sz) {
        st->sz--;
        if (st->cap > 8 && st->sz <= st->cap/4) {
            st->cap /= 2;
            if (!(st->st = realloc(st->st, st->cap*sizeof(struct host_ent*)))) {
                pthread_mutex_unlock(&h->parent->pending_lk);
                return 0;
            }
        }

        pthread_mutex_unlock(&h->parent->pending_lk);
        return st->st[st->sz];
    }

    pthread_mutex_unlock(&h->parent->pending_lk);
    return 0;
}

