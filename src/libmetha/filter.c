/*-
 * filter.c
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

#include "filter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/** 
 * Evaluate a URL against the various rules
 * of the given filter. Return LM_FILTER_ALLOW
 * if the URL is accepted, or LM_FILTER_DENY
 * if we should deny it. If an error occurs,
 * return LM_FILTER_ERROR.
 **/
int
lm_filter_eval_url(filter_t *f, url_t *url)
{
    int x;
    struct rule *r;

    for (x=0; x<f->num_rules; x++) {
        r = &f->rules[x];
        if (umex_match(url, r->expr)) {
            if (r->allow == LM_FILTER_ALLOW)
                return LM_FILTER_ALLOW;
#ifdef DEBUG
            fprintf(stderr, "* filter:(%p) denied '%s'\n", f, url->str);
#endif
            return LM_FILTER_DENY;
        }

    }

    return LM_FILTER_ALLOW;
}

/** 
 * Add a rule in the form of a UMEX.
 *
 * A URL matching a UMEX rule is DENIED.
 **/
M_CODE
lm_filter_add_rule(filter_t *f, int what, umex_t *expr)
{
    int x;
    
    if (!f->num_rules) {
        f->num_rules = 0;
        if (!(f->rules = malloc(sizeof(struct rule))))
            return M_OUT_OF_MEM;
    } else {
        if (!(f->rules = realloc(f->rules, (f->num_rules+1)*sizeof(struct rule))))
            return M_OUT_OF_MEM;
    }

    if (what == LM_FILTER_ALLOW) {
        /* find the first deny-rule in the list, and swap this allow-rule with that one */
        for (x=0; x<f->num_rules; x++) {
            if (f->rules[x].allow == LM_FILTER_DENY) {
                f->rules[f->num_rules] = f->rules[x];
                break;
            }
        }
    } else
        x = f->num_rules;

    f->rules[x].expr = expr;
    f->rules[x].allow = what;

    f->num_rules ++;
    return M_OK;
}

