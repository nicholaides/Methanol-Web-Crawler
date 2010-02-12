/*-
 * io.h
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

/** 
 * Primary input/output module
 *
 * Used by libmetha for input/output to network,
 * local filesystem etc.
 *
 * Uses libcurl for network, and standard fopen() (etc)
 * along with readdir() (etc) for local file system
 **/

#ifndef _METHA_IO__H_
#define _METHA_IO__H_

#include <time.h>
#include <curl/curl.h>
#include <pthread.h>
#include "errors.h"
#include "url.h"

enum {
    LM_IOMSG_ADD,
    LM_IOMSG_STOP,
};

/** 
 * describe info such as 
 * content-type, file size, which 
 * protoocl used, and so on
 **/
typedef struct iostat {
    short   status_code; /* status code, dependant on protocol */
    struct {
        char *location;
        char *content_type;
    } headers;
} iostat_t;

typedef struct iobuf {
    char *ptr;
    size_t sz;
    size_t cap;
} iobuf_t;

/** 
 * Temporary data stored for each concurrent
 * transfer over network. Stores a unique ID given 
 * by a worker so the worker can identify this
 * transfer later.
 *
 * ioh will be used internally by the IO-functions,
 * and then handle will be used externally by 
 * workers to get information. Before an ioprivate_t 
 * object is returned to a worker, handle will be 
 * set to the CURL handle used.
 **/
typedef struct ioprivate {
    int        identifier;
    union {
        struct iohandle *ioh;
        CURL            *handle;
    };
} ioprivate_t;

typedef struct iohandle {
    CURL       *primary;
    struct io  *io;
    iobuf_t     buf;
    iostat_t    transfer;

    struct {
        ioprivate_t **list;
        size_t        count;
        size_t        allocsz;
    } done;
    int waiting;
    int total; /* total transfers active/done */
    int provided;
    pthread_mutex_t dcond_mtx;
    pthread_cond_t  dcond;
} iohandle_t;

/* pos descriptor for the pending transfer queue */
typedef struct ioqp {
    int   identifier;
    char *url;
    iohandle_t *ioh; /* which handle added this url? */
} ioqp_t;

typedef struct io {
    M_CODE     error;
    CURLM     *multi_h;
    CURLSH    *share_h;
    pthread_t  thr;
    int        started;

    struct metha *m;

    /* pipe used for communicating with the IO-thread, see
     * LM_IOMSG_* */
    int msg_fd[2];
    int e_fd;
    int e_timeout;

    /* timer */
    int          synchronous;
    clock_t      timer_last;
    unsigned int timer_wait;
    unsigned int timer_wait_mp;

    /* running transfers */
    int        prev_running;
    int        nrunning;

    struct {
        ioqp_t *pos;
        int size;
        int allocsz;
    } queue;

    pthread_mutex_t queue_mtx;
    pthread_rwlock_t share_mtx;
    pthread_rwlock_t cookies_mtx;
    pthread_rwlock_t dns_mtx;

    /* options */
    int         cookies;
    int         verbose;
    const char *user_agent; /* free()'d externally */
    const char *proxy;
} io_t;

struct ioev_t {

};

iohandle_t *lm_iohandle_obtain(io_t *io);
void        lm_iohandle_destroy(iohandle_t *ioh);
M_CODE      lm_io_get(iohandle_t *h, url_t *url);
M_CODE      lm_io_head(iohandle_t *h, url_t *url);
M_CODE      lm_io_save(iohandle_t *h, url_t *url, const char *name);
M_CODE      lm_iothr_stop(io_t *io);
M_CODE      lm_io_provide(iohandle_t *h, const char *buf, size_t len);

int lm_io_data_cb(char *ptr, size_t size, size_t nmemb, void *s);

#endif

