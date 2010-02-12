/*-
 * urlengine.h
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

#ifndef _METHA_URLENGINE__H_
#define _METHA_URLENGINE__H_

#include "url.h"
#include "mtrie.h"
#include "utable.h"
#include "filter.h"
#include <pthread.h>

#define UE_SECONDARY_SIZE 64

struct host_ent {
    char            *str; /* host name */
    uint16_t         len; /* length of host name */
    int              rfetched;
    mtrie_t          cache;
    ulist_t          list;
    struct host_ent *next;
    pthread_mutex_t  lock;
    filter_t         filter;

    /* if this host name is pending to be crawled */
    uint8_t          pending;
    unsigned int     pending_pos;

    uint8_t          secondary_n;
};

/* stack for pending external urls */
struct lm_pending_e_st {
    struct host_ent **st;
    unsigned int      sz;
    unsigned int      cap;
};

typedef struct ue {
    struct {
        /**
         * There are actually 512 linked lists with host entries, 
         * but the thought of 512 rwlocks scares me, so we split up
         * the locks, 8 linked lists per lock. Each rwlock will 
         * most of the time be locked for read anyway, and only
         * locked for write when a completely new host is found
         *
         * Note that when a worker is actively crawling a specific 
         * host, it will save a direct pointer to its cache, hence
         * this lock will not prevent workers from adding to their 
         * current caches.
         **/
        pthread_rwlock_t rwlock;
        struct host_ent *hosts[8];
    } secondary[UE_SECONDARY_SIZE];

    pthread_mutex_t        pending_lk;
    struct lm_pending_e_st pending;
} ue_t;

typedef struct uehandle {
    utable_t      primary;
    ue_t         *parent;
    url_t        *current;
    void         *state_info;
    struct host_ent *host_ent;
    struct host_ent *host_ent_bk;

    int           is_peeking;
    unsigned int  depth_counter;
    unsigned int  depth_limit;
    unsigned int  depth_counter_bk; /* backup values when doing external peeking */
    unsigned int  depth_limit_bk;
} uehandle_t;

M_CODE ue_init(ue_t *ue);
M_CODE ue_add(uehandle_t *h, const char *url, uint16_t len);
M_CODE ue_revert(uehandle_t *h, const char *url, uint16_t len);
M_CODE ue_add_initial(uehandle_t *h, const char *url, uint16_t len);
M_CODE ue_set_host(uehandle_t *h, const char *host, uint16_t host_sz);
M_CODE ue_move_to_secondary(uehandle_t *h, url_t *url);
void  *ue_get_state_info(uehandle_t *h);
void   ue_set_state_info(uehandle_t *h, void *info);
void   ue_handle_free(uehandle_t *h);
void   ue_uninit(ue_t *ue);
const char *ue_next(uehandle_t *h);
uehandle_t *ue_handle_obtain(ue_t *ue);
struct host_ent* ue_pop_pending(uehandle_t *h);
M_CODE ue_set_hostent(uehandle_t *h, struct host_ent *ent);

#endif

