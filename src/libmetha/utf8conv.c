/*-
 * utf8conv.c
 * This file is part of libmetha
 *
 * Copyright (c) 2009, Emil Romanus <emil.romanus@gmail.com>
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
#include <stdlib.h>
#include <iconv.h>
#include <ctype.h>

#include "errors.h"
#include "worker.h"
#include "urlengine.h"
#include "io.h"

/** 
 * Convert the downloaded data to utf8
 **/
M_CODE
lm_parser_utf8conv(worker_t *w, iobuf_t *buf,
                   uehandle_t *ue_h, url_t *url,
                   attr_list_t *al)
{
    char *enc;
    char *s;
    char *p;
    char *e;
    size_t      oleft, ileft;
    char       *out, *outp, *in;
    iconv_t     cd = (iconv_t)-1;
    M_CODE      r = M_OK;

    /* we'll look in the Content-Type http header
     * and see if the character encoding was 
     * set there */
    if ((enc = w->io_h->transfer.headers.content_type)
            && (s = strstr(enc, "charset="))) {
        s+=8;
        /* the encoding should now be at s, don't
         * do any conversion if the encoding is
         * already UTF-8 */
        if (strncasecmp(s, "UTF-8", 5) != 0)
            cd = iconv_open("UTF-8", s);
    } else {
        /* either content-type was not set, or it did not
         * contain the 'charset=' string. Our next hope is 
         * to look in the HTML for a meta tag for replacing
         * content-type and setting the charset */
        for (s=buf->ptr, e=buf->ptr+buf->sz;
                s<e;) {
            if (*s == '<') {
                do s++; while (isspace(*s));
                if (strncasecmp(s, "meta", 4) != 0)
                    continue;
                s+=4;
                if (!(p = memchr(s, '>', e-s)))
                    break;
                if (!(p = memmem(s, p-s, "charset=", 8)))
                    continue;
                p+=8;
                for (s=p; isalnum(*s) || *s == '-'; s++)
                    ;
                char tmp = *s;
                *s = '\0';
                cd = iconv_open("UTF-8", p);
                *s = tmp;
                break;
            } else
                s++;
        }
    }
    /* XXX: Question: If the charset is set using the
     *      HTTP header Content-Type, but then replaced
     *      using a <meta> tag, should we allow this?
     **/
    if (cd == (iconv_t)-1)
        return M_FAILED;

    oleft = buf->sz*2;
    in = buf->ptr;
    ileft = buf->sz;

    if (!(out = outp = malloc(oleft+1)))
        return M_OUT_OF_MEM;

    if (iconv(cd, &in, &ileft, &outp, &oleft) == -1)
        r = M_FAILED;

    /* replace the pointer in buf with our new,
     * converted buffer, free the old one */
    buf->cap = outp-out;
    buf->sz = outp-out;
    free(buf->ptr);
    /* free unused space */
    if (!(buf->ptr = realloc(out, buf->sz)))
        return M_OUT_OF_MEM;

    iconv_close(cd);
    return r;
}

