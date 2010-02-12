/*-
 * io.c
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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>

#include "metha.h"
#include "default.h"

#define BUF_INIT_SIZE 1024
#define QUEUE_INIT_SIZE 8
#define LM_IO_MAX_RETRIES 3

#ifdef WIN32
 #include <windows.h>
 #define sec_sleep(x) Sleep(x*1000)
#else
 #include <unistd.h>
 #define sec_sleep(x) sleep(x)
#endif

static void *lm_iothr_main(io_t *io);
static int   lm_iothr_socket_cb(CURL *h, curl_socket_t s, int action, void *userp, void *socketp);
static int   lm_iothr_set_timer_cb(CURLM *m, long timeout, io_t *io);
static void  lm_iothr_check_completed(io_t *io);
static size_t lm_iothr_data_cb(void *ptr, size_t size, size_t nmemb, void *s);
static M_CODE lm_iothr_check_pending(io_t *io);
static void  lm_iothr_wait(io_t *io, int mp);
static M_CODE lm_io_perform_http(iohandle_t *h, url_t *url);
static M_CODE lm_io_perform_ftp(iohandle_t *h, url_t *url);
static M_CODE lm_io_no_perform(iohandle_t *h, url_t *url);

static void lm_iothr_lock_shared_cb(CURL *h, curl_lock_data data, curl_lock_access access, void *ptr);
static void lm_iothr_unlock_shared_cb(CURL *h, curl_lock_data data, void *ptr);

/* keep in sync with LM_PROTOCOL_* in url.h */
static M_CODE (*__perform[])(iohandle_t *, url_t *) = {
    &lm_io_perform_http,
    &lm_io_perform_ftp,
    &lm_io_no_perform,
    &lm_io_no_perform,
    &lm_io_no_perform,
    &lm_io_no_perform,
};

/** 
 * Initializes the input/output part of the 
 * specified metha object
 **/
M_CODE
lm_init_io(io_t *io, metha_t *m)
{
    io->m = m;
#ifdef WIN32
    io->synchronous = 1;
#endif

    if (io->synchronous)
        return M_OK;

    if (!(io->multi_h = curl_multi_init())) {
        LM_ERROR(m, "could not create CURL multi interface");
        return M_FAILED;
    }
    if (!(io->share_h = curl_share_init())) {
        LM_ERROR(m, "could not create CURL share interface");
        return M_ERROR;
    }

    if (pipe(io->msg_fd) != 0) {
        LM_ERROR(m, "pipe() failed: %s", strerror(errno));
        return M_FAILED;
    }

    if (!(io->queue.pos = malloc(QUEUE_INIT_SIZE*sizeof(ioqp_t))))
        return M_OUT_OF_MEM;

    io->queue.allocsz = QUEUE_INIT_SIZE;
    io->queue.size = 0;

    io->started = 0;

    pthread_mutex_init(&io->queue_mtx, 0);
    pthread_rwlock_init(&io->cookies_mtx, 0);
    pthread_rwlock_init(&io->dns_mtx, 0);
    pthread_rwlock_init(&io->share_mtx, 0);

    if (io->cookies)
        curl_share_setopt(io->share_h, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(io->share_h, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(io->share_h, CURLSHOPT_LOCKFUNC, &lm_iothr_lock_shared_cb);
    curl_share_setopt(io->share_h, CURLSHOPT_UNLOCKFUNC, &lm_iothr_unlock_shared_cb);
    curl_share_setopt(io->share_h, CURLSHOPT_USERDATA, io);

    curl_multi_setopt(io->multi_h, CURLMOPT_SOCKETFUNCTION, &lm_iothr_socket_cb);
    curl_multi_setopt(io->multi_h, CURLMOPT_SOCKETDATA, io);
    curl_multi_setopt(io->multi_h, CURLMOPT_TIMERFUNCTION, &lm_iothr_set_timer_cb);
    curl_multi_setopt(io->multi_h, CURLMOPT_TIMERDATA, io);
    /*curl_multi_setopt(io->multi_h, CURLMOPT_PIPELINING, 1);*/

    return M_OK;
}

/** 
 * clean up input/output module
 **/
void
lm_uninit_io(io_t *io)
{
    if (!io->synchronous) {
        if (io->multi_h)
            curl_multi_cleanup(io->multi_h);
        if (io->share_h)
            curl_share_cleanup(io->share_h);
        if (io->queue.pos)
            free(io->queue.pos);

        pthread_mutex_destroy(&io->queue_mtx);
        pthread_rwlock_destroy(&io->cookies_mtx);
        pthread_rwlock_destroy(&io->dns_mtx);
        pthread_rwlock_destroy(&io->share_mtx);

        close(io->msg_fd[0]);
        close(io->msg_fd[1]);
    }
}

/** 
 * Obtain an IO-handle
 * 
 * Each worker will need its own handle to 
 * the IO-system. The handle is derived from
 * the io-object initialized at metha_t->io.
 *
 * The handler is nothing but a pointer to the 
 * originial io-object, along with a unique 
 * CURL handle for "primary" asynchronous
 * operations
 **/
iohandle_t *
lm_iohandle_obtain(io_t *io)
{
    iohandle_t *ioh = calloc(1, sizeof(iohandle_t));
    if (ioh) {
        if (!(ioh->primary = curl_easy_init())) {
            free(ioh);
            return 0;
        }
        ioh->io = io;

        if (!io->synchronous) {
            if (!(ioh->done.list = malloc(sizeof(ioprivate_t*)*24)))
                return 0;
            ioh->done.allocsz = 24;

            pthread_mutex_init(&ioh->dcond_mtx, 0);
            pthread_cond_init(&ioh->dcond, 0);

            curl_easy_setopt(ioh->primary, CURLOPT_SHARE, io->share_h);
            curl_easy_setopt(ioh->primary, CURLOPT_WRITEFUNCTION, (curl_write_callback)&lm_io_data_cb);
        }

        if (io->proxy)
            curl_easy_setopt(ioh->primary, CURLOPT_PROXY, io->proxy);
        if (io->verbose)
            curl_easy_setopt(ioh->primary, CURLOPT_VERBOSE, 1);
        if (io->cookies)
            curl_easy_setopt(ioh->primary, CURLOPT_COOKIEFILE, "");

        curl_easy_setopt(ioh->primary, CURLOPT_USERAGENT, io->user_agent);
        curl_easy_setopt(ioh->primary, CURLOPT_ENCODING, "");

        if (!(ioh->buf.ptr = malloc(BUF_INIT_SIZE)))
            return 0;
        ioh->buf.cap = BUF_INIT_SIZE;

        ioh->transfer.headers.content_type = "";
    }
    return ioh;
}

/** 
 * Destroy the given iohandle and free its member blah
 **/
void
lm_iohandle_destroy(iohandle_t *ioh)
{
    if (!ioh->io->synchronous) {
        pthread_mutex_destroy(&ioh->dcond_mtx);
        pthread_cond_destroy(&ioh->dcond);

        if (ioh->done.list)
            free(ioh->done.list);
    }

    if (ioh->buf.ptr)
        free(ioh->buf.ptr);

    curl_easy_cleanup(ioh->primary);
    free(ioh);
}

/** 
 * Used when workers share cookies and DNS info, this function
 * is called by libcurl to lock cookie/dns access
 **/
static void
lm_iothr_lock_shared_cb(CURL *h,
                        curl_lock_data data,
                        curl_lock_access access,
                        void *ptr)
{
    io_t *io = (io_t*)ptr;
    pthread_rwlock_t *m;

    (void)h;

    switch (data) {
        case CURL_LOCK_DATA_SHARE:    m = &io->share_mtx; break;
        case CURL_LOCK_DATA_DNS:      m = &io->dns_mtx; break;
        case CURL_LOCK_DATA_COOKIE:   m = &io->cookies_mtx; break;
        default:                      LM_WARNING(io->m, "something borked! :("); return;
    }

    switch (access) {
        case CURL_LOCK_ACCESS_SINGLE: pthread_rwlock_wrlock(m); return;
        case CURL_LOCK_ACCESS_SHARED: pthread_rwlock_rdlock(m); return;
        default:                      LM_WARNING(io->m, "something borked! :("); return;
    }

    return;
}
/** 
 * Used when workers share cookies and DNS info, this function
 * is called by libcurl to unlock cookie/dns access
 **/
static void
lm_iothr_unlock_shared_cb(CURL *h,
                        curl_lock_data data,
                        void *ptr)
{
    io_t *io = (io_t*)ptr;
    
    switch (data) {
        case CURL_LOCK_DATA_SHARE:    pthread_rwlock_unlock(&io->share_mtx); return;
        case CURL_LOCK_DATA_DNS:      pthread_rwlock_unlock(&io->dns_mtx); return;
        case CURL_LOCK_DATA_COOKIE:   pthread_rwlock_unlock(&io->cookies_mtx); return;
        default:                      LM_WARNING(io->m, "something borked! :("); return;
    }

    return;
}

/** 
 * Used by the PRIMARY transfer handle of an iohandle_t
 **/
int 
lm_io_data_cb(char *ptr, size_t size,
              size_t nmemb, void *s)
{
    iobuf_t *b = (iobuf_t*)s;
    size_t  sz = nmemb*size;

    if (b->sz+sz > b->cap) {
        do {
            b->cap *= 2;
        } while (b->cap < b->sz+sz);

        if (!(b->ptr = realloc(b->ptr, b->cap)))
            return !sz;
    }

    memcpy(b->ptr+b->sz, ptr, sz);
    b->sz+=sz;

    return sz;
}
/** 
 * Used by the PRIMARY transfer handle of an iohandle_t,
 * write the data to a file instead of to memory
 **/
int 
lm_io_data_save_cb(char *ptr, size_t size,
                   size_t nmemb, void *s)
{
    FILE *fp = (FILE*)s;
    return fwrite(ptr, size, nmemb, fp);
}

/** 
 * Perform an HTTP HEAD on the given URL.
 **/
M_CODE
lm_io_head(iohandle_t *h, url_t *url)
{
    if (h->io->synchronous)
        lm_iothr_wait(h->io, 1);

    memset(&h->transfer, 0, sizeof(iostat_t));

    /* head is supported for HTTP and HTTPS only
     * XXX: https is disabled for now */
    if (url->protocol != LM_PROTOCOL_HTTP)
        return M_OK;

    curl_easy_setopt(h->primary, CURLOPT_URL, url->str);
    curl_easy_setopt(h->primary, CURLOPT_NOBODY, 1);
    curl_easy_setopt(h->primary, CURLOPT_HEADER, 1);
    curl_easy_setopt(h->primary, CURLOPT_WRITEFUNCTION, &lm_iothr_data_cb);
    curl_easy_setopt(h->primary, CURLOPT_WRITEDATA, 0);

    return (__perform[url->protocol])(h, url);
}

/** 
 * Provide a buffer. If a buffer is provided and lm_io_get()
 * is called, no transfer will be performed but the provided
 * buffer will be returned as if downloaded by lm_io_get().
 **/
M_CODE
lm_io_provide(iohandle_t *h, const char *buf, size_t len)
{
    h->buf.sz = 0;
    h->buf.ptr[0] = '\0';

    lm_io_data_cb((char*)buf, 1, len, &h->buf);

    h->provided = 1;
    return M_OK;
}

/** 
 * Download the given URL to a local file with
 * the given name.
 **/
M_CODE
lm_io_save(iohandle_t *h, url_t *url,
           const char *name)
{
    FILE  *fp;
    M_CODE r;
    /* TODO: support provided data by writing
     *       it to the file */
    if (h->io->synchronous)
        lm_iothr_wait(h->io, 0);

    h->buf.sz = 0;
    h->buf.ptr[0] = '\0';

    memset(&h->transfer, 0, sizeof(iostat_t));

    if (!(fp = fopen(name, "w+"))) 
        return M_COULD_NOT_OPEN;

    curl_easy_setopt(h->primary, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(h->primary, CURLOPT_WRITEFUNCTION, &lm_io_data_save_cb);
    curl_easy_setopt(h->primary, CURLOPT_NOBODY, 0);
    curl_easy_setopt(h->primary, CURLOPT_URL, url->str);

    r = __perform[url->protocol](h, url);

    fclose(fp);
    return r;
}

/** 
 * Download the given URL. This function might be affected 
 * by a previous call to lm_io_provide().
 **/
M_CODE
lm_io_get(iohandle_t *h, url_t *url)
{
    if (h->provided) {
        h->provided = 0;
        return M_OK;
    }

    if (h->io->synchronous)
        lm_iothr_wait(h->io, 0);

    h->buf.sz = 0;
    h->buf.ptr[0] = '\0';

    memset(&h->transfer, 0, sizeof(iostat_t));

    curl_easy_setopt(h->primary, CURLOPT_WRITEDATA, &h->buf);
    curl_easy_setopt(h->primary, CURLOPT_WRITEFUNCTION, &lm_io_data_cb);
    curl_easy_setopt(h->primary, CURLOPT_NOBODY, 0);
    curl_easy_setopt(h->primary, CURLOPT_URL, url->str);

    return __perform[url->protocol](h, url);
}

/** 
 * Function called when perform is done on a URL with 
 * an unsupported protocol.
 **/
static M_CODE
lm_io_no_perform(iohandle_t *h, url_t *url)
{
    return M_FAILED;
}

/** 
 * Perform over FTP
 **/
static M_CODE
lm_io_perform_ftp(iohandle_t *h, url_t *url)
{
    CURLcode c;
    int retries = 0;
    int done = 0;
    
    do {
        c = curl_easy_perform(h->primary);
        switch (c) {
            case CURLE_OK:
                done = 1;
                break;

            case CURLE_FTP_CANT_GET_HOST:
            case CURLE_COULDNT_RESOLVE_HOST:
            case CURLE_COULDNT_CONNECT:
            case CURLE_OPERATION_TIMEDOUT:
            case CURLE_GOT_NOTHING:
                if (retries < LM_IO_MAX_RETRIES) {
                    retries ++;
                    LM_WARNING(h->io->m, "retry %d of %d (%s)", retries, LM_IO_MAX_RETRIES, url->str);
                    done = 0;
                    break;
                }

            default:
                LM_WARNING(h->io->m, "%s (%s)", curl_easy_strerror(c), url->str);
                done = 1;
                return M_FAILED;
        }
    } while (!done);

    return M_OK;
}

/** 
 * Perform a transfer over HTTP and handle return data 
 * such as content-type.
 **/
static M_CODE
lm_io_perform_http(iohandle_t *h, url_t *url)
{
    CURLcode c;
    int retries = 0;
    int done = 0;
    long status;

#if LIBCURL_VERSION_NUM < 0x71202
    curl_easy_setopt(h->primary, CURLOPT_FOLLOWLOCATION, 1);
#endif

    do {
        c = curl_easy_perform(h->primary);
        switch (c) {
            case CURLE_OK:
                /**
                 * Everything went just like it should, but we should
                 * check for possible redirects
                 **/
                if (curl_easy_getinfo(h->primary, CURLINFO_RESPONSE_CODE, &status) == CURLE_OK) {
                    h->transfer.status_code = status;
                    if (status >= 300 && status < 400) {
                        /**
                         * We got a redirect, and we add it to the list of headers so 
                         * the worker can read it later
                         **/
                        const char *new_url;
                        if (curl_easy_getinfo(h->primary, CURLINFO_REDIRECT_URL, &new_url) == CURLE_OK)
                            h->transfer.headers.location = (char*)new_url;
                    }
                    curl_easy_getinfo(h->primary, CURLINFO_CONTENT_TYPE, &h->transfer.headers.content_type);
                    if (!h->transfer.headers.content_type)
                        h->transfer.headers.content_type = "";
                }
                done = 1;
                break;

            case CURLE_COULDNT_RESOLVE_HOST:
            case CURLE_COULDNT_CONNECT:
            case CURLE_OPERATION_TIMEDOUT:
            case CURLE_GOT_NOTHING:
                if (retries < LM_IO_MAX_RETRIES) {
                    retries ++;
                    LM_WARNING(h->io->m, "retry %d of %d (%s)", retries, LM_IO_MAX_RETRIES, url->str);
                    done = 0;
                    break;
                }

            default:
                LM_WARNING(h->io->m, "%s (%s)", curl_easy_strerror(c), url->str);
                return M_FAILED;
        }
    } while (!done);

    return M_OK;
}

/** 
 * Wait for any transfer to finish, return the used CURL 
 * interface, from which we will later extract information from
 *
 * XXX: This function must NOT be called when running synchronously.
 **/
ioprivate_t *
lm_multipeek_wait(iohandle_t *ioh)
{
    ioprivate_t *ret;

    pthread_mutex_lock(&ioh->dcond_mtx);

#ifdef IO_DEBUG
    fprintf(stderr, "* iohandle:(%p) lm_multipeek_wait, total = %d\n", ioh, ioh->total);
#endif

    if (!ioh->total) {
        pthread_mutex_unlock(&ioh->dcond_mtx);
        return 0; /* done transfers will decrease the total amount each by one,
                     when total reaches 0, it means all transfers are finished */
    }
    if (!ioh->done.count) {
        ioh->waiting = 1;
        /* no done transfers, wait for the io-thread to send a condition */
        pthread_cond_wait(&ioh->dcond, &ioh->dcond_mtx);
        ioh->waiting = 0;
    }
    ret = ioh->done.list[ioh->done.count-1];
    ioh->done.count--;
    ioh->total--;
    pthread_mutex_unlock(&ioh->dcond_mtx);

    return ret;
}

/** 
 * If the mode option was set, we need to sleep some
 * time between transfers.
 **/
static void
lm_iothr_wait(io_t *io, int mp)
{
    clock_t now = clock();
    clock_t done;
    if (mp)
        done = io->timer_last+io->timer_wait_mp;
    else 
        done = io->timer_last+io->timer_wait;

    if (now < done && done-now > CLOCKS_PER_SEC)
        sec_sleep((done-now)/CLOCKS_PER_SEC);
}

/** 
 * Add a URL to the multi-lookup loop, return whether it was added
 * successfully. Data/status must be gathered later through 
 * lm_multipeek_wait(). 'id' is a unique number to associate with this
 * URL.
 *
 * XXX: This function must NOT be called when running synchronously.
 **/
M_CODE
lm_multipeek_add(iohandle_t *ioh, url_t *url, int id)
{
    int msg;
#ifdef IO_DEBUG
    fprintf(stderr, "* iohandle:(%p) lm_multipeek_add: '%s'\n", ioh, url->str);
#endif

    io_t *io = ioh->io;

    /* XXX: temporary fix to prevent dead locks */
    if (url->protocol != LM_PROTOCOL_HTTP)
        return M_FAILED;

    pthread_mutex_lock(&io->queue_mtx);
    /* add this url to the queue */
    if (io->queue.size+1 >= io->queue.allocsz) {
        io->queue.allocsz *= 2;
        io->queue.pos = realloc(io->queue.pos, sizeof(ioqp_t)*io->queue.allocsz);
        if (!io->queue.pos) {
            pthread_mutex_unlock(&io->queue_mtx);
            return M_OUT_OF_MEM;
        }
    }
    io->queue.pos[io->queue.size].url = url->str;
    io->queue.pos[io->queue.size].ioh = ioh;
    io->queue.pos[io->queue.size].identifier = id;
    io->queue.size++;
    pthread_mutex_unlock(&io->queue_mtx);
    ioh->total++;
    /* inform our event loop, a new url was added */
    msg = LM_IOMSG_ADD;

    if (write(io->msg_fd[1], &msg, sizeof(int)) <= 0)
        return M_IO_ERROR;
    return M_OK;
}

M_CODE
lm_iothr_stop(io_t *io)
{
    int msg = LM_IOMSG_STOP;
    if (!io->synchronous && io->started) {
        /* signal the event loop to exit */
        if (write(io->msg_fd[1], &msg, sizeof(int)) <= 0)
            return M_IO_ERROR;
        /* wait for the thread to exit */
        pthread_join(io->thr, 0);
    }

    return M_OK;
}

/** 
 * Callbakc function upon receiving data from libcurl for any 
 * transfer. We simply discard everything, since we don't really 
 * need anything of it. All we need is the Content-Type, and 
 * libcurl will provide us that through curl_easy_getinfo()
 **/
static size_t
lm_iothr_data_cb(void *ptr, size_t size, size_t nmemb, void *s)
{
    return nmemb*size;
}

/** 
 * called when the epoll() timer has been reached
 **/
static M_CODE
lm_iothr_timer_reached(io_t *io)
{
#ifdef IO_DEBUG
    fprintf(stderr, "* io:(%p) timer reached\n", io);
#endif

    while (curl_multi_socket_action(io->multi_h,
                CURL_SOCKET_TIMEOUT, 0, &io->nrunning)
            == CURLM_CALL_MULTI_PERFORM)
        ;

    lm_iothr_check_completed(io);

    if (!io->nrunning)
        io->e_timeout = 20000;

    return M_OK;
}

/** 
 * Called by libcurl to set a timeout
 **/
static int 
lm_iothr_set_timer_cb(CURLM *m, long timeout, io_t *io)
{
#ifdef IO_DEBUG
    fprintf(stderr, "* io:(%p) set timer to %d ms\n",
              io, (int)timeout);
#endif
    io->e_timeout = (int)timeout;
    return 0;
}
/** 
 * Check how many transfers that have been completed
 * since the last check, and put all completed 
 * transfers in their corresponding iohandle's "done"-
 * queue
 **/
static void
lm_iothr_check_completed(io_t *io)
{
    int      n_msgs;
    CURL    *h;
    CURLMsg *msg;
    ioprivate_t *info;
    iohandle_t  *ioh;

#ifdef IO_DEBUG
    fprintf(stderr, "* io:(%p) lm_iothr_check_completed(), prev = %d, curr = %d\n", 
            io, io->prev_running, io->nrunning);
#endif

    if (io->prev_running > io->nrunning) {
        while ((msg = curl_multi_info_read(io->multi_h, &n_msgs))) {
            if (msg->msg == CURLMSG_DONE) {
                h = msg->easy_handle;
                curl_easy_getinfo(h, CURLINFO_PRIVATE, &info);
                curl_multi_remove_handle(io->multi_h, h);
#ifdef IO_DEBUG
                fprintf(stderr, "* io:(%p) remove handle %p, id = '%d'\n", io, h, info->identifier);
#endif

                ioh = info->ioh;

                /* dont waste time gathering further information here,
                 * let the iohandle do that in its own thread. Hence, 
                 * we simply mark the transfer as done and remove the 
                 * handle from the multi stack */

                /* TODO: use trylock to avoid blocking, we should 
                 * put blocking operations in a temporary queue and
                 * attempt them later until the operation doesnt block
                 * or max retries have been reached */
                pthread_mutex_lock(&ioh->dcond_mtx);
                if (ioh->done.count+1 >= ioh->done.allocsz) {
                    ioh->done.allocsz *= 2;
                    if (!(ioh->done.list = realloc(ioh->done.list, ioh->done.allocsz*sizeof(ioprivate_t*)))) {
                        LM_ERROR(ioh->io->m, "out of mem");
                        abort();
                    }
                }
                /* set the info->handle to the CURL handle used, so the 
                 * worker will be able to fetch information about it */
                info->handle = h;
                ioh->done.list[ioh->done.count] = info;
                ioh->done.count++;
                if (ioh->waiting) 
                    pthread_cond_signal(&ioh->dcond);
                pthread_mutex_unlock(&ioh->dcond_mtx);
            }
        }
    }
    io->prev_running = io->nrunning;
}

/**
 * Called by libcurl to inform us about new and removed 
 * sockets.
 **/
static int
lm_iothr_socket_cb(CURL *h, curl_socket_t s, int action,
                   void *userp, void *socketp)
{
    io_t *io = (io_t*)userp;
    int e_fd = io->e_fd;

    struct epoll_event ev;
#ifdef IO_DEBUG
    const char *what[] = {"none", "IN", "OUT", "INOUT", "REMOVE"};
    fprintf(stderr,
            "* io:(%p) lm_iothr_socket_cb(), action '%s' on%s socket %d, handle %p\n",
            io, what[action], (socketp?"":" NEW"), s, h);
#endif

    if (action == CURL_POLL_REMOVE)
        epoll_ctl(e_fd, EPOLL_CTL_MOD, s, &ev);
    else {
        ev.data.fd = (int)s;
        ev.events = ((action & CURL_POLL_IN) ? EPOLLIN : 0)
                   |((action & CURL_POLL_OUT) ? EPOLLOUT : 0);
        if (!socketp) {
            epoll_ctl(e_fd, EPOLL_CTL_ADD, s, &ev);
            curl_multi_assign(io->multi_h, s, (void*)1);
        } else
            epoll_ctl(e_fd, EPOLL_CTL_MOD, s, &ev);
    }

    return 0;
}

/** 
 * data is available for read/write on the given file descriptor
 **/
static M_CODE
lm_iothr_fd_event(io_t *io, int fd, int events)
{
    CURLMcode  c;
    int        ev_bitmask; /* events to libcurl */
#ifdef IO_DEBUG
    fprintf(stderr,
            "* io:(%p) lm_iothr_fd_event(), events = '%s%s%s', socket %d\n",
            io, 
            ((events & EPOLLIN) ? "READ":""),
            ((events & EPOLLOUT) ? "WRITE":""),
            ((events & EPOLLERR) ? "ERROR":""),
            fd);
#endif

    if (events & EPOLLERR) {
        /* an error occurred */
        curl_multi_socket_action(io->multi_h, fd,
                CURL_CSELECT_ERR, &io->nrunning);

        return M_SOCKET_ERROR;
    } else {
        ev_bitmask = ((events & EPOLLIN) ? CURL_CSELECT_IN : 0)
                     | ((events & EPOLLOUT) ? CURL_CSELECT_OUT : 0);

        while ((c = curl_multi_socket_action(io->multi_h, fd,
                        ev_bitmask, &io->nrunning))
                ==  CURLM_CALL_MULTI_PERFORM)
            ;

        if (c != CURLM_OK)
            return M_SOCKET_ERROR;
    }

    lm_iothr_check_completed(io);
    return M_OK;
}

/** 
 * Call this function to check for pending 
 * transfers. If any pending transfer is found, it
 * will be added.
 **/
static M_CODE
lm_iothr_check_pending(io_t *io)
{
    CURL *h;
    int   x;

    pthread_mutex_lock(&io->queue_mtx);
    for (x=0; x<io->queue.size; x++) {
#ifdef IO_DEBUG
        fprintf(stderr, "* io:(%p) lm_iothr_check_pending: '%s', id: '%d'\n",
                io,
                io->queue.pos[x].url, io->queue.pos[x].identifier);
#endif
        ioprivate_t *tmp = malloc(sizeof(ioprivate_t));
        if (!tmp)
            break;

        tmp->ioh = io->queue.pos[x].ioh;
        tmp->identifier = io->queue.pos[x].identifier;

        h = curl_easy_init();
        curl_easy_setopt(h, CURLOPT_URL, io->queue.pos[x].url);
        curl_easy_setopt(h, CURLOPT_PRIVATE, tmp);
        curl_easy_setopt(h, CURLOPT_NOBODY, 1);
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &lm_iothr_data_cb);
        curl_easy_setopt(h, CURLOPT_USERAGENT, io->user_agent);
        curl_easy_setopt(h, CURLOPT_ENCODING, "");
        curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1);
        if (io->proxy)
            curl_easy_setopt(h, CURLOPT_PROXY, io->proxy);
        if (io->verbose)
            curl_easy_setopt(h, CURLOPT_VERBOSE, 1);
        if (io->cookies)
            curl_easy_setopt(h, CURLOPT_COOKIEFILE, "");
#ifdef IO_DEBUG
        fprintf(stderr, "* io:(%p) add handle %p, id: '%d'\n",
                io, h, io->queue.pos[x].identifier);
#endif
        curl_multi_add_handle(io->multi_h, h);
    }
    io->queue.size=0;
    pthread_mutex_unlock(&io->queue_mtx);

    return M_OK;
}

/** 
 * Launch the I/O thread used for the multipeek loop. Note that
 * the i/o thread is not launched if running synchronously.
 **/
M_CODE
lm_iothr_launch(io_t *io)
{
    if (!io->synchronous) {
        if (pthread_create(&io->thr, 0, (void *(*)(void*))&lm_iothr_main, io) != 0)
            return M_FAILED;
        io->started = 1;
    }

#ifdef DEBUG
    else fprintf(stderr, "* io:(%p) timer enabled, no I/O thread launched\n", io);
#endif

    return M_OK;
}

/** 
 * entry point of the IO-thread, will listen for events on sockets
 * connected to libcurl interfaces and also for signals from the main
 * thread
 **/
static void *
lm_iothr_main(io_t *io)
{
    struct epoll_event msg_ev;
    struct epoll_event events[8];

    int stop = 0;
    int n; /* num fds */
    int x;
    int msg;

    io->e_timeout = 10000;

#ifdef DEBUG
    fprintf(stderr, "* io:(%p) started\n", io);
#endif
    io->prev_running = 0;
#ifdef IO_DEBUG
    fprintf(stderr, "* io:(%p) waiting for events\n", io);
#endif

    if ((io->e_fd = epoll_create(1024)) < 0) {
        LM_ERROR(io->m, "epoll_create failed");
        return 0;
    }

    /* add our messaging fd so we can start listening for messages
     * from the main thread*/
    msg_ev.data.fd = io->msg_fd[0];
    msg_ev.events  = EPOLLIN;
    if (epoll_ctl(io->e_fd, EPOLL_CTL_ADD, io->msg_fd[0], &msg_ev) != 0) {
        LM_ERROR(io->m, "epoll_ctl failed");
        return 0;
    }

    while (!stop) {
        n = epoll_wait(io->e_fd, events, 8, io->e_timeout);
        
        if (n == 0) {
            lm_iothr_timer_reached(io);
            continue;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        for (x=0; x<n; x++) {
            if (events[x].data.fd == io->msg_fd[0]) {
                /* message received */
                if ((read(events[x].data.fd, &msg, sizeof(int))) != sizeof(int)
                        || msg == LM_IOMSG_STOP) {
                    stop = 1;
                    break;
                }
                if (msg == LM_IOMSG_ADD)
                    lm_iothr_check_pending(io);
            } else
                lm_iothr_fd_event(io, events[x].data.fd, events[x].events);
        }
    }

#ifdef DEBUG
    fprintf(stderr, "* io:(%p) stopped\n", io);
#endif
    return 0;
}

