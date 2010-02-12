/*-
 * url.h
 * This file is part of libmetha
 *
 * Copyright (c) 2008, 2009, Emil Romanus <emil.romanus@gmail.com>
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

#ifndef _LM_URL__H_
#define _LM_URL__H_

#include "errors.h"
#include <stdint.h>

/* Protocols, don't modify this without keeping __perform in io.c in sync */
enum {
    LM_PROTOCOL_HTTP = 0,
    LM_PROTOCOL_FTP,
    LM_PROTOCOL_FILE,
    LM_PROTOCOL_HTTPS,
    LM_PROTOCOL_FTPS,
    LM_PROTOCOL_UNKNOWN,
    LM_NUM_PROTOCOLS
};

/* url flags */
#define LM_URL_DYNAMIC    1
#define LM_URL_EXTERNAL   2
#define LM_URL_WWW_PREFIX 4

#define LM_URL_ISSET(x,y) ((x)->flags & (y))

/** 
 * Every found URL is put into a url structure. The related functions
 * will encode the URL and set some values to describe it.
 *
 * Since version 1.4.99, the domain name length is now limited to 255
 * characters:
 * RFC1034: "The total number of octets  that represent a domain name
 *           (i.e., the sum of all label octets and label lengths) is
 *           limited to 255."
 * 
 **/
typedef struct url {
    char *str;
    uint16_t allocsz, file_o,
             sz,      ext_o;
    uint8_t  host_o, host_l;
    uint8_t  bind, protocol;
    uint8_t  flags;
} url_t;

void   lm_url_init(url_t *url);
void   lm_url_uninit(url_t *url);
void   lm_url_dump(url_t *url);
M_CODE lm_url_set(url_t *url, const char *str, uint16_t size);
M_CODE lm_url_combine(url_t *dest, url_t *source, const char *str, uint16_t len);
M_CODE lm_url_dup(url_t *dest, url_t *source);
int    lm_url_hostcmp(url_t *u1, url_t *u2);
void   lm_url_nullify(url_t *url);
void   lm_url_swap(url_t *u1, url_t *u2);

#define lm_url_bind(url, ft) (url)->bind = (ft)

#endif
