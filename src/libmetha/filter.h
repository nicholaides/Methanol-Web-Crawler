/*-
 * filter.h
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

#ifndef _LM_FILTER__H_
#define _LM_FILTER__H_

#include <stdint.h>
#include "url.h"
#include "umex.h"

enum {
    LM_FILTER_ALLOW,
    LM_FILTER_DENY,
    LM_FILTER_ERROR,
};

/** 
 * Uses UMEX to filter URLs.
 *
 * Each hostent will have its own filter, this adds support
 * for robots.txt.
 *
 * Try to keep this structure as small as possible!
 **/

struct rule {
    uint8_t  allow;
    umex_t  *expr;
};

typedef struct filter {
    uint32_t      num_rules;
    struct rule  *rules;
} filter_t;

int    lm_filter_eval_url(filter_t *f, url_t *url);
M_CODE lm_filter_add_rule(filter_t *f, int what, umex_t *expr);

#endif

