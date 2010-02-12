/*-
 * url.c
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

#include "url.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NUM_PROTOCOLS (sizeof(protocols)/sizeof(struct protocol))
#define ENC_STACK_SIZE 8

struct protocol {
    char   *str;
    int     str_len;
    uint8_t val;
};

static struct protocol protocols[] = {
    {"http",  4, LM_PROTOCOL_HTTP},
    {"ftp",   3, LM_PROTOCOL_FTP},
    {"file",  4, LM_PROTOCOL_FILE},
    {"https", 5, LM_PROTOCOL_HTTPS},
    {"ftps",  4, LM_PROTOCOL_FTPS},
};

static M_CODE lm_url_realloc(url_t *url, uint16_t sz);
static M_CODE lm_url_encodecpy(url_t *url, const char *prefix, uint16_t prefix_sz, const char *str, uint16_t sz);

/** 
 * Zero an url
 **/
void
lm_url_nullify(url_t *url)
{
    if (url->str)
        url->str[0] = '\0';
    url->sz = 0;
    url->flags = 0;
}

/** 
 * Make an exact copy of soruce to dest
 **/
M_CODE
lm_url_dup(url_t *dest, url_t *source)
{
    if (lm_url_realloc(dest, source->allocsz) == M_OK) {
        memcpy(dest->str, source->str, source->sz);
        dest->file_o = source->file_o;
        dest->host_o = source->host_o;
        dest->host_l = source->host_l;
        dest->ext_o  = source->ext_o;
        dest->bind   = source->bind;
        dest->sz     = source->sz;
        dest->protocol = source->protocol;
        dest->flags = source->flags;
        return M_OK;
    }

    return M_FAILED;
}

/** 
 * Placement-initialize a url struct
 **/
void
lm_url_init(url_t *url)
{
    url->sz = 0;
    url->allocsz = 0;
    url->str = 0;
}

/** 
 * Return 0 if host name matches
 **/
int
lm_url_hostcmp(url_t *u1, url_t *u2)
{
    url_t *tmp;

    if (u1->host_l != u2->host_l) {
        if (u1->host_l < u2->host_l) {
            tmp = u1;
            u1 = u2;
            u2 = tmp;
        }
        if (u1->host_l - u2->host_l == 4) {
            if (strncasecmp(u1->str+u1->host_o, "www.", 4) == 0)
                return strncasecmp(u1->str+u1->host_o+4, u2->str+u2->host_o, u2->host_l);
        }
        return -2;
    } else
        return strncasecmp(u1->str+u1->host_o, u2->str+u2->host_o, u1->host_l);
}

/**
 * Free data pointed to by url, DO NOT free the url object itself
 **/
void
lm_url_uninit(url_t *url)
{
    free(url->str);
    url->sz = 0;
    url->allocsz = 0;
    url->str = 0;
}

/** 
 * Reallocate the target URL so that it 
 * can hold 'sz' amount of bytes.
 **/
static M_CODE
lm_url_realloc(url_t *url, uint16_t sz)
{
    int a = sz;
    char *s = url->str;
    if (a < 16)
        a = 16;
    else {
        /* round up size to closest power of two */
        a--;
        a |= a >> 1;
        a |= a >> 2;
        a |= a >> 4;
        a |= a >> 8;
        a++;
    }
    if (url->allocsz < a) {
        s = realloc(s, a);
        if (!s) {
            /* allocation failed, try the tightest buffer (not
             * rounded up to closest power of two)
             * */
            if (!(s = realloc(s, sz + 1)))
                return M_OUT_OF_MEM;
            url->allocsz = sz + 1;
        } else
            url->allocsz = a;
    } else if (a >= 64 && url->allocsz >= (a << 4)) {
        s = realloc(s, (url->allocsz >> 2));
        if (!s)
            return M_OUT_OF_MEM;
        url->allocsz >>= 2;
    }
    url->str = s;
    return M_OK;
}

/** 
 * Set the given URL 'url' to the given string 'str' of length 'size'
 **/
M_CODE
lm_url_set(url_t *url, const char *str, uint16_t size)
{
    char *e = (char*)str + size;
    char *s = (char*)str;
    int x, y;
    uint8_t  proto = 0xff;
    url->flags = 0;

    while (isalnum(*s))
        s++; /* loop past protocol */
    if (*s == ':') {
        y = s - str;
        /* match protocol */
        for (x = 0; x < NUM_PROTOCOLS; x++) {
            if (protocols[x].str_len == y
                && *str == *protocols[x].str
                && strncasecmp(str+1, protocols[x].str+1,
                               protocols[x].str_len-1) == 0) {
                proto = protocols[x].val;
                break;
            }
        }
        if (proto == 0xff) {
            proto = LM_PROTOCOL_UNKNOWN;
            /* TODO: currently we discard unknown protocols... we should probably have 
             * a user-supplied list of protocols so the user can handle them */
            return M_FAILED;
        }
    } else {/* url did not start with a protocol, this is NOT allowed. */
        lm_url_nullify(url);
        return M_FAILED;
    }

    do {
        s++;
        if (s >= e)
            return M_FAILED; /* fail on urls like "http://" only */
    } while (*s == '/'); /* find start of host name */

    url->host_o = (uint8_t)(s-str); 

    for (;;) { /* find end of host name */
        s++;
        if (s >= e) {
            /* url ends here? this happens for urls like "http://domain.com",
             * that is, it's got no ending slash. We fix this easily. */
            url->host_l = (s-str)-url->host_o;
            url->file_o = s-str;
            url->ext_o  = 0;

            if (url->host_l > 4 && strncasecmp(str+url->host_o, "www.", 4) == 0)
                url->flags |= LM_URL_WWW_PREFIX;

            /* combine the hostname with a single '/' */
            lm_url_encodecpy(url, str, s-str, "/", 1);

            url->protocol = proto;
            url->bind     = 0;

            return M_OK;
        }

        if (!isalnum(*s)) {
            if (*s == '/')
                break;
            if (*s != '.' && *s != '-' && *s != ':') {
                /* don't allow weird characters */
                lm_url_nullify(url);
                return M_FAILED;
            }
        }
    }

    /* ok, now we're past the host name, and there was a '/' after the host name */
    url->host_l = (uint8_t)(s-str - url->host_o);
    url->file_o = (uint16_t)(s-str);
    url->ext_o  = 0;

    if (url->host_l > 4 && strncasecmp(str+url->host_o, "www.", 4) == 0)
        url->flags |= LM_URL_WWW_PREFIX;

    lm_url_encodecpy(url, str, s-str, s, e-s);

    url->protocol = proto;
    url->bind     = 0;

    return M_OK;
}

/** 
 * lm_url_combine() expects path and file information 
 * in 'str'. It adds this information to the URL 'source'
 * and puts the result in 'dest'.
 *
 * If 'str' begins with a '/', this function will only 
 * copy the host name (and protocol) from 'source'. 
 * Otherwise, it will append 'str' after the last '/' in
 * source. Examples:
 *
 * if source = "http://google.com/abc/" and str = "xyz.htm"
 * then dest = "http://google.com/abc/xyz.htm"
 *
 * if source = "http://google.com/abc/" and str = "/xyz.htm"
 * then dest = "http://google.com/xyz.htm"
 **/
M_CODE
lm_url_combine(url_t *dest, url_t *source,
               const char *str, uint16_t len)
{
    /* XXX: we assume 'source' is *at least* a protocol + a host name */
    uint16_t offs;

    if (*str == '/') {
        offs = source->host_o+source->host_l;
        dest->file_o = offs;
    } else {
        offs = source->file_o+1;
        dest->file_o = source->file_o;
    }

    dest->host_o = source->host_o;
    dest->host_l = source->host_l;
    dest->protocol = source->protocol;
    dest->ext_o = 0;
    dest->flags = (source->flags & ~LM_URL_DYNAMIC);

    return lm_url_encodecpy(dest, source->str, offs, str, len);
}

/** 
 * lm_url_encodecpy() URL-encodes 'str' and copies it to
 * the target URL 'url'. 'prefix' is a plain string 
 * used as a prefix when copying the buffer, and it will
 * NOT be URL-encoded by this function, but copied directly.
 *
 * When this function is invoked from lm_url_set(),
 * 'prefix' will contain the protocol and the host name
 * of the URL. Ex. "http://google.com/"
 *
 * When invoked from lm_url_combine(), 'prefix' will 
 * contain at least the protocol and the host name of 
 * the URL, but it might also contain already-encoded 
 * path structures. Ex. "http://google.com/random/"
 **/
static M_CODE
lm_url_encodecpy(url_t *url, const char *prefix,
                 uint16_t prefix_sz, const char *str,
                 uint16_t sz)
{
    uint16_t  file_o = 0, ext_o = 0;
    uint8_t   is_dyn = 0;
    char     *s = (char*)str;
    char     *e = (char*)str+sz;
    char     *t;
    char     *t_e;

    int c;

    lm_url_realloc(url, prefix_sz+sz+16);

    /* copy the untouched prefix */
    memcpy(url->str, prefix, prefix_sz);

    t = url->str+prefix_sz;
    t_e = url->str+url->allocsz-prefix_sz;

    for (; s<e; s++) {
        if (t+4>=t_e) {
            int o = t-url->str;
            lm_url_realloc(url, url->allocsz+(e-s+16));
            t = url->str+o;
            t_e = url->str+url->allocsz;
        }
        if (*s <= 0x20 || (*s & 0x80)) {
            *t++ = '%';
            *t++ = ((c = (*s >> 4)) > 0x09 ? c + '7' : c + '0');
            *t++ = ((c = (*s & 0x0F)) > 0x09 ? c + '7' : c + '0');
            continue;
        } else if (*s == '?') {
            is_dyn = 1;
            /* TODO: also encode the data after the '?' */
            for (; s<e; s++) { /* loop through evertyhing after the '?' */
                if (*s == ' ')
                    *t++ = '+';
                else if (*s == '&' && (*(s+1)) == 'a' && (*(s+2)) == 'm'
                        && (*(s+3)) == 'p' && (*(s+4)) == ';') {
                    *t++ = '&';
                    s+=4;
                    continue;
                } else if (*s == '#') { /* break URL if a '#' if found */
                    break;
                } else
                    *t++ = *s;
            }
            break;
        } else if (*s == '#') { /* cut the URL here, we don't care for what's behind the '#' */
            break;
        } else if (*s == '/') {
            while (*(s+1) == '/') {
                s++;
            }
            if (*(s+1) == '.') {
                if (*(s+2) == '.' && (*(s+3) == '/' || !*(s+3))) {
pback:
                    if (t > url->str+url->host_o+url->host_l+1) {
                        for (t--;;t--) {
                            if (t<=url->str+url->host_o+url->host_l
                                    || (*t == '/' && t != url->str+prefix_sz-1))
                                break;
                        }
                    }
                    *t = *(s+3);
                    s+=2;
                    continue;
                } else if (*(s+2) == '/')
                    s+=2;
                else if (!*(s+2))
                    s++;
            }
            file_o = t-url->str;
            ext_o  = 0; /* clear file extension offset */
        } else if (*s == '.') {
            if (s == str && *(s+1) == '.' && *(s+2) == '/') {
                s--;
                goto pback;
            } else if (*(s+1) == '/') {
                s+=1;
                continue;
            }
            ext_o = t-url->str;
        }

        *t++ = *s;
    }

    sz = t-url->str;

    /* insert a null-byte for stdlib compatibility */
    url->str[sz] = '\0';
    /* save the rest of the information (host info is already saved) */
    url->sz       = sz;
    url->flags    |= is_dyn;
    if (ext_o)
        url->ext_o    = ext_o;
    if (file_o)
        url->file_o   = file_o;

    return M_OK;
}

void
lm_url_swap(url_t *u1, url_t *u2)
{
    url_t tmp = *u1;
    (*u1) = (*u2);
    (*u2) = tmp;
}

