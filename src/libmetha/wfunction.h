/*-
 * wfunction.h
 * This file is part of libmetha
 *
 * Copyright (c) 2008-2009, Emil Romanus <emil.romanus@gmail.com>
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

#ifndef _LM_WFUNCTION__H_
#define _LM_WFUNCTION__H_

/** 
 * Parser Callback Syntax:
 * M_CODE name(worker_t *, iobuf_t *, uehandle_t *, url_t *, attr_list_t *al);
 *
 * Handler Callback Syntax:
 * M_CODE name(worker_t *, iohandle_t *, url_t *);
 *
 * See io.h for iobuf_t.
 *
 * See builtin.c for the default text parser, html.c for
 * the html parser.
 **/

#include <stdint.h>
#include <jsapi.h>

#include "errors.h"

#define LM_NUM_BUILTIN_PARSERS (sizeof(m_builtin_parsers)/sizeof(wfunction_t))

/* function types for wfunction_t */
enum {
    LM_WFUNCTION_TYPE_NATIVE = 0,
    LM_WFUNCTION_TYPE_JAVASCRIPT,
};

/* function purposes (parser, handler) */
enum {
    LM_WFUNCTION_PURPOSE_HANDLER = 0,
    LM_WFUNCTION_PURPOSE_PARSER,
};

struct worker;
struct iobuf;
struct iohandle;
struct url;
struct uehandle;
struct attr_list;

/** 
 * Worker functions
 *
 * Used by workers to declare and store function information 
 * about javascript and native C functions used as handlers 
 * and parsers.
 **/
typedef struct wfunction {
    char       *name;
    uint8_t     type;
    uint8_t     purpose;
    union {
        M_CODE    (*native_parser)(struct worker *, struct iobuf *, struct uehandle *, struct url *, struct attr_list *);
        M_CODE    (*native_handler)(struct worker *, struct iohandle *, struct url *);
        /*JSFunction *javascript;*/
        /* ok so the people at mozilla changed the PURPOSE of the JSFunction
         * type in js-1.8.0, so we can't use it... JS_CallFunction segfaults
         * in 1.8.0 segfaults. However, calling the function its jsval 
         * seems to work in both 1.7.0 and 1.8.0 */
        jsval javascript;
    } fn;
} wfunction_t;

#endif
