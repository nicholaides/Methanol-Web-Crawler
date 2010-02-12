/*-
 * conf.c
 * This file is part of Methanol
 * http://metha-sys.org/
 * http://bithack.se/projects/methabot/
 *
 * Copyright (c) 2009, Emil Romanus <sdac@bithack.se>
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
 */

#include "lmc.h"
#include "conf.h"

#include <string.h>

static struct filetype *filetype_create(const char *name, uint32_t name_len);
static struct filetype *filetype_find(void *unused, const char *name);
static M_CODE filetype_add(void *unused, struct filetype *ft);
static M_CODE filetype_copy(struct filetype* dest, struct filetype* source);
static void filetype_clear(struct filetype* ft);
static M_CODE filetype_set_attributes(struct filetype *ft, char **attributes, int num_attributes);

static struct crawler *crawler_create(const char *name, uint32_t name_len);
static struct crawler *crawler_find(void *unused, const char *name);
static M_CODE crawler_add(void *unused, struct crawler *cr);
static M_CODE crawler_copy(struct crawler* dest, struct crawler* source);
static void crawler_clear(struct crawler* cr);

/** 
 * Skeleton filetype class used for loading classes from 
 * configuration files, the 'attribute' option is currently
 * the only interesting field, all other fields are
 * discarded. By setting the callback function or variable
 * offset to 0, the lmc parser will discard the option value
 * but not see it as an error.
 **/
struct lmc_class
nol_m_filetype_class = {
    .name = "filetype",
    .add_cb  = &filetype_add,
    .find_cb = &filetype_find,
    .zero_cb = &filetype_clear,
    .copy_cb = &filetype_copy,
    .constructor_cb = &filetype_create,
    .destructor_cb = &nol_m_filetype_destroy,
    .flags_offs = 0,
    . opts = {
        LMC_OPT_ARRAY("extensions", 0),
        LMC_OPT_ARRAY("mimetypes", 0),
        LMC_OPT_STRING("parser", 0),
        LMC_OPT_STRING("handler", 0),
        LMC_OPT_EXTRA("expr", 0),
        LMC_OPT_STRING("crawler_switch", 0),
        LMC_OPT_ARRAY("attributes", &filetype_set_attributes),
        LMC_OPT_FLAG("ignore_host", 1),
        LMC_OPT_END,
    }
};

/** 
 * Crawler objects will discard all options, but
 * each crawler will be saved in the database
 **/
struct lmc_class
nol_m_crawler_class = 
{
    .name           = "crawler",
    .add_cb         = &crawler_add,
    .find_cb        = &crawler_find,
    .zero_cb        = &crawler_clear,
    .copy_cb        = &crawler_copy,
    .constructor_cb = &crawler_create,
    .destructor_cb  = &nol_m_crawler_destroy,
    .flags_offs     = 0,
    .opts = {
        LMC_OPT_ARRAY("filetypes", 0),
        LMC_OPT_STRING("dynamic_url", 0),
        LMC_OPT_STRING("extless_url", 0),
        LMC_OPT_STRING("unknown_url", 0),
        LMC_OPT_STRING("dir_url", 0),
        LMC_OPT_STRING("ftp_dir_url", 0),
        LMC_OPT_FLAG("external", 0),
        LMC_OPT_UINT("external_peek", 0),
        LMC_OPT_UINT("depth_limit", 0),
        LMC_OPT_STRING("initial_filetype", 0),
        LMC_OPT_STRING("init", 0),
        LMC_OPT_FLAG("spread_workers", 0),
        LMC_OPT_FLAG("jail", 0),
        LMC_OPT_FLAG("robotstxt", 0),
        LMC_OPT_STRING("default_handler", 0),
        LMC_OPT_END,
    }
};

/** 
 * Add the filetype to 'srv.filetypes'
 **/
static M_CODE
filetype_add(void *unused, struct filetype *ft)
{
    if (!srv.num_filetypes)
        srv.filetypes = malloc(sizeof(void*));
    else
        srv.filetypes = realloc(srv.filetypes, sizeof(void*)*(srv.num_filetypes+1));

    if (!srv.filetypes)
        return M_OUT_OF_MEM;

    srv.filetypes[srv.num_filetypes] = ft;
    srv.num_filetypes ++;

    return M_OK;
}

static struct filetype *
filetype_create(const char *name, uint32_t name_len)
{
    struct filetype *r;

    if (!(r = calloc(1, sizeof(struct filetype))))
        return 0;

    if (!(r->name = malloc(name_len+1))) {
        free(r);
        return 0;
    }

    memcpy(r->name, name, name_len);
    r->name[name_len] = '\0';
    return r;
}

/* find the given filetype in srv.filetypes */
static struct filetype *
filetype_find(void *unused, const char *name)
{
    int x;

    for (x=0; x<srv.num_filetypes; x++)
        if (strcmp(name, srv.filetypes[x]->name) == 0)
            return srv.filetypes[x];

    return 0;
}

/**
 * Copy over the filetype attributes
 **/
static M_CODE
filetype_copy(struct filetype* dest, struct filetype* source)
{
    int x;

    dest->num_attributes = source->num_attributes;
    dest->attributes = malloc(sizeof(char *)*dest->num_attributes);
    for (x=0; x<dest->num_attributes; x++)
        dest->attributes[x] = strdup(source->attributes[x]);

    return M_OK;
}

static void
filetype_clear(struct filetype* ft)
{
    int x;
    
    if (ft->num_attributes) {
        for (x=0; x<ft->num_attributes; x++)
            free(ft->attributes[x]);

        ft->num_attributes = 0;
    }
}

void
nol_m_filetype_destroy(struct filetype* ft)
{
    filetype_clear(ft);
    free(ft->name);
    free(ft);
}

static M_CODE
filetype_set_attributes(struct filetype *ft,
                        char **attributes, int num_attributes)
{
    int x;

    if (ft->num_attributes) {
        for (x=0; x<ft->num_attributes; x++)
            free(ft->attributes[x]);
        free(ft->attributes);
    }

    ft->attributes     = attributes;
    ft->num_attributes = num_attributes;

    return M_OK;
}

/** 
 * Add the crawler to 'srv.crawlers'
 **/
static M_CODE
crawler_add(void *unused, struct crawler *cr)
{
    if (!srv.num_crawlers)
        srv.crawlers = malloc(sizeof(void*));
    else
        srv.crawlers = realloc(srv.crawlers, sizeof(void*)*(srv.num_crawlers+1));

    if (!srv.crawlers)
        return M_OUT_OF_MEM;

    srv.crawlers[srv.num_crawlers] = cr;
    srv.num_crawlers ++;

    return M_OK;
}

static struct crawler *
crawler_create(const char *name, uint32_t name_len)
{
    struct crawler *r;

    if (!(r = calloc(1, sizeof(struct crawler))))
        return 0;

    if (!(r->name = malloc(name_len+1))) {
        free(r);
        return 0;
    }

    memcpy(r->name, name, name_len);
    r->name[name_len] = '\0';
    return r;
}

/* find the given crawlwer in srv.crawlers */
static struct crawler *
crawler_find(void *unused, const char *name)
{
    int x;

    for (x=0; x<srv.num_crawlers; x++)
        if (strcmp(name, srv.crawlers[x]->name) == 0)
            return srv.crawlers[x];

    return 0;
}

static M_CODE
crawler_copy(struct crawler* dest, struct crawler* source)
{
    return M_OK;
}

static void
crawler_clear(struct crawler* cr)
{

}

void
nol_m_crawler_destroy(struct crawler* cr)
{
    free(cr->name);
    free(cr);
}
