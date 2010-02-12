/*-
 * attr.h
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

#ifndef _LM_ATTR__H_
#define _LM_ATTR__H_

#include "errors.h"

/** 
 * Attributes are used to bind specific information to a found URL
 * matching a specific filetype. Parsers will fill in attribute
 * values, and if the application is connected to a methanol system,
 * the attributes will be used when uploading the URL as a match.
 *
 * The target function (see LMOPT_TARGET_FUNCTION) will receive
 * a pointer to an attr_list_t.
 **/

typedef struct attr {
    const char *name;  /* externally allocated, the lm_attr_* functions
                          does not touch the buffer pointed to by this
                          pointer
                          */

    char       *value; /* this attributes current value, will be allocated
                          and free'd automatically by the lm_attr_* functions
                         */
    unsigned   size;

    /**
     * NOTE: attributes are ALWAYS stored internally as char buffers,
     *       the type of the attribute is only relevant when uploading
     *       data to a methanol system
     **/
} attr_t;

typedef struct attr_list {
    attr_t *list;
    int     num_attributes;
    int     changed; /* 1 if any elemnt in the attr_list has been
                        changed since the last call to 
                        lm_attrlist_prepare() */
} attr_list_t;

/* attr.c */
M_CODE lm_attribute_set(attr_list_t *al, const char *attribute, const char *value, unsigned size);
M_CODE lm_attrlist_prepare(attr_list_t *al, const char **attributes, unsigned num_attributes);
void   lm_attrlist_cleanup(attr_list_t *al);

#endif
