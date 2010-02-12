/*-
 * errors.c
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

#include "errors.h"
#include "metha.h"
#include "worker.h"

#include <stdio.h>
#include <string.h>

const char *
lm_strerror(M_CODE c)
{
    switch (c) {
        case M_OK:                  return "no error";
        case M_OUT_OF_MEM:          return "out of memory";
        case M_COULD_NOT_OPEN:      return "could not open";
        case M_IO_ERROR:            return "input/output error";
        case M_SOCKET_ERROR:        return "socket error";
        case M_NO_CRAWLERS:         return "no crawlers defined";
        case M_NO_FILETYPES:        return "no filetypes defined";
        case M_FT_MULTIDEF:         return "multiple definitions of filetype";
        case M_CR_MULTIDEF:         return "multiple definitions of crawler";
        case M_NO_DEFAULT_CRAWLER:  return "no default crawler provided";
        case M_BAD_OPTION:          return "bad option";
        case M_BAD_VALUE:           return "bad value";
        case M_UNKNOWN_FILETYPE:    return "unknown filetype";
        case M_EXTERNAL_ERROR:      return "external error";
        case M_TOO_BIG:             return "too big";
        case M_SYNTAX_ERROR:        return "syntax error";
        case M_NOT_READY:           return "not ready";
        case M_UNKNOWN_CRAWLER:     return "unknown crawler";
        case M_EVENT_ERROR:         return "internal event loop error";
        case M_UNRESOLVED:          return "unresolved symbol";
        case M_THREAD_ERROR:        return "internal multi-threading error";

        case M_ERROR:               return "error";
        default:                    return "unknown error";
    }
}

/** 
 * The application using libmetha should 
 * provide its own message handling functions. But 
 * these are the default ones, printing to stdout and
 * stderr directly.
 **/
void
lm_default_error_reporter(metha_t *m, const char *s, ...)
{
    int len;
    char *p;

    va_list va;
    va_start(va, s);

    len = strlen(s);

    if (!(p = malloc(len+8)))
        return;

    memcpy(p, "error: ", 7);
    memcpy(p+7, s, len+1);

    vfprintf(stderr, p, va);

    free(p);

    va_end(va);
}

void
lm_default_target_reporter(metha_t *m, worker_t *w, url_t *url,
                           attr_list_t *attr, filetype_t *ft)
{
    printf("target: %s\n", url->str);
}

void
lm_default_status_reporter(metha_t *m, worker_t *w, url_t *url)
{
    printf("url: %s\n", url->str);
}

void
lm_default_warning_reporter(metha_t *m, const char *s, ...)
{
    int len;
    char *p;

    va_list va;
    va_start(va, s);

    len = strlen(s);

    if (!(p = malloc(len+10)))
        return;

    memcpy(p, "warning: ", 9);
    memcpy(p+9, s, len+1);

    vfprintf(stderr, p, va);

    free(p);

    va_end(va);
}

