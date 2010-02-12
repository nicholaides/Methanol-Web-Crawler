/*-
 * crawler.c
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

#include "crawler.h"
#include <string.h>

crawler_t *
lm_crawler_create(const char *name, uint32_t nlen)
{
    crawler_t *cr = calloc(1, sizeof(crawler_t));
    if (!cr)
        return 0;
    if (!(cr->name = malloc(nlen+1))) {
        free(cr);
        return 0;
    }
    memcpy(cr->name, name, nlen);
    cr->name[nlen] = '\0';

    lm_url_init(&cr->jail_url);

    /*
    cr->filetypes = 0;
    cr->num_filetypes = 0;
    cr->flags = 0;
    cr->init = 0;
    cr->peek_limit = 0;
    */
    cr->depth_limit = 1;

    return cr;
}

void
lm_crawler_destroy(crawler_t *c)
{
    lm_crawler_clear(c);
    lm_ftindex_destroy(&c->ftindex);

    free(c->name);
    free(c);
}

/** 
 * Duplicate source by copying its settings to dest.
 **/
M_CODE
lm_crawler_dup(crawler_t *dest, crawler_t *source)
{
    int x;

    if (dest == source)
        return M_OK;

    lm_crawler_clear(dest);

    if (source->num_filetypes) {
        if (!(dest->filetypes = malloc(source->num_filetypes*sizeof(char *))))
            return M_OUT_OF_MEM;

        for (x=0; x<source->num_filetypes; x++)
            dest->filetypes[x] = strdup(source->filetypes[x]);
    }
    dest->num_filetypes = source->num_filetypes;

    dest->flags = source->flags;

    if (source->init) dest->init = strdup(source->init);

    if (!lm_crawler_flag_isset(dest, LM_CRFLAG_PREPARED)) {
        if (source->initial_filetype.name) dest->initial_filetype.name = strdup(source->initial_filetype.name);
        if (source->ftindex.dynamic_url) dest->ftindex.dynamic_url = strdup(source->ftindex.dynamic_url);
        if (source->ftindex.dir_url) dest->ftindex.dir_url = strdup(source->ftindex.dir_url);
        if (source->ftindex.extless_url) dest->ftindex.extless_url = strdup(source->ftindex.extless_url);
        if (source->ftindex.unknown_url) dest->ftindex.unknown_url = strdup(source->ftindex.unknown_url);
        if (source->ftindex.ftp_dir_url) dest->ftindex.ftp_dir_url = strdup(source->ftindex.ftp_dir_url);
    } else {
        if (source->initial_filetype.ptr)
            dest->initial_filetype.ptr = (void*)source->initial_filetype.name;
        dest->ftindex.dynamic_url = source->ftindex.dynamic_url;
        dest->ftindex.dir_url = source->ftindex.dir_url;
        dest->ftindex.extless_url = source->ftindex.extless_url;
        dest->ftindex.unknown_url = source->ftindex.unknown_url;
        dest->ftindex.ftp_dir_url = source->ftindex.ftp_dir_url;
    }

    dest->peek_limit = source->peek_limit;

    return M_OK;
}

/** 
 * Required by lmetha_load_config for the override keyword
 **/
void
lm_crawler_clear(crawler_t *c)
{
    int x;

    if (c->num_filetypes) {
        for (x=0; x<c->num_filetypes; x++)
            free(c->filetypes[x]);
        free(c->filetypes);
        c->num_filetypes = 0;
    }

    if (c->init) {
        free(c->init);
        c->init = 0;
    }

    if (!lm_crawler_flag_isset(c, LM_CRFLAG_PREPARED)) {
        if (c->initial_filetype.name) {
            free(c->initial_filetype.name);
            c->initial_filetype.name = 0;
        }
    }

    c->flags = 0;
    c->peek_limit = 0;
    c->depth_limit = 1;
}

/** 
 * Add the name of a filetype to the list of filetypes
 * that applies to this crawler.
 **/
M_CODE
lm_crawler_add_filetype(crawler_t *cr, const char *name)
{
    if (!cr->num_filetypes)
        cr->filetypes = malloc(sizeof(const char *));
    else
        cr->filetypes = realloc(cr->filetypes, (cr->num_filetypes+1)*sizeof(const char*));

    if (!cr->filetypes)
        return M_OUT_OF_MEM;

    if (!(name = strdup(name)))
        return M_OUT_OF_MEM;

    cr->filetypes[cr->num_filetypes] = (char *)name;
    cr->num_filetypes ++;

    return M_OK;
}

/** 
 * Set the list of filetypes that this crawler will use
 *
 * Used by lmetha_load_config() when parsing configuration
 * files.
 **/
M_CODE
lm_crawler_set_filetypes(crawler_t *cr, char **filetypes, int num_filetypes)
{
    int x;

    if (cr->num_filetypes) {
        for (x=0; x<cr->num_filetypes; x++)
            free(cr->filetypes[x]);
        free(cr->filetypes);
    }

    cr->filetypes = filetypes;
    cr->num_filetypes = num_filetypes;

    return M_OK;
}

