/*-
 * filetype.c
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

#include "filetype.h"

#include <stdlib.h>
#include <string.h>

filetype_t *
lm_filetype_create(const char *name, uint32_t nlen)
{
    filetype_t *ret = malloc(sizeof(filetype_t));
    if (!ret)
        return 0;
    if (!(ret->name = malloc(nlen+1))) {
        free(ret);
        return 0;
    }

    memcpy(ret->name, name, nlen);
    ret->name[nlen]   = '\0';
    ret->e_count      = 0;
    ret->m_count      = 0;
    ret->attr_count   = 0;
    ret->parser_str   = 0;
    ret->expr         = 0;
    ret->flags        = 0;
    ret->switch_to.name = 0;
    ret->parser_chain.parsers = 0;
    ret->parser_chain.num_parsers = 0;
    ret->handler.name = 0;
    ret->counter = 0;

#if HAVE_BUILTIN_ATOMIC == 0
    pthread_mutex_init(&ret->counter_lk, 0);
#endif

    return ret;
}

void
lm_filetype_destroy(filetype_t *ft)
{
    free(ft->name);
    lm_filetype_clear(ft);
#if HAVE_BUILTIN_ATOMIC == 0
    pthread_mutex_destroy(&ft->counter_lk);
#endif
    free(ft);
}

/** 
 * Required by lmetha_load_config() for the override keyword,
 * also used by the lm_filetype_destroy() function
 **/
void
lm_filetype_clear(filetype_t *ft)
{
    int x;

    if (ft->expr) {
        umex_free(ft->expr);
        ft->expr = 0;
    }

    if (ft->e_count) {
        for (x=0; x<ft->e_count; x++)
            free(ft->extensions[x]);
        free(ft->extensions);
        ft->e_count = 0;
    }
    if (ft->m_count) {
        for (x=0; x<ft->m_count; x++)
            free(ft->mimetypes[x]);
        free(ft->mimetypes);
        ft->m_count = 0;
    }
    if (ft->attr_count) {
        for (x=0; x<ft->attr_count; x++)
            free(ft->attributes[x]);
        free(ft->attributes);
        ft->attr_count = 0;
    }
    if (ft->parser_chain.num_parsers) {
        ft->parser_chain.num_parsers = 0;
        free(ft->parser_chain.parsers);
    }

    if (ft->parser_str) {
        free(ft->parser_str);
        ft->parser_str = 0;
    }

    ft->switch_to.name = 0;
    ft->flags = 0;
}

M_CODE
lm_filetype_dup(filetype_t *dest, filetype_t *source)
{
    int x;

    if (dest == source)
        return M_OK;

    lm_filetype_clear(dest);

    if (source->e_count) {
        if (!(dest->extensions = malloc(source->e_count*sizeof(char*))))
            return M_OUT_OF_MEM;
        for (x=0; x<source->e_count; x++)
            dest->extensions[x] = strdup(source->extensions[x]);
    }
    if (source->m_count) {
        if (!(dest->mimetypes = malloc(source->m_count*sizeof(char*))))
            return M_OUT_OF_MEM;
        for (x=0; x<source->m_count; x++)
            dest->mimetypes[x] = strdup(source->mimetypes[x]);
    }
    if (source->attr_count) {
        if (!(dest->attributes = malloc(source->attr_count*sizeof(char*))))
            return M_OUT_OF_MEM;
        for (x=0; x<source->attr_count; x++)
            dest->attributes[x] = strdup(source->attributes[x]);
    }

    dest->e_count = source->e_count;
    dest->m_count = source->m_count;
    dest->attr_count = source->attr_count;
    if (source->expr)
        dest->expr = umex_dup(source->expr);
    if (source->switch_to.name)
        dest->switch_to.name = strdup(source->switch_to.name);
    if (source->parser_chain.num_parsers) {
        parser_chain_t *ch_d = &dest->parser_chain,
                       *ch_s = &source->parser_chain;
        ch_d->num_parsers = ch_s->num_parsers;
        if (!(ch_d->parsers = malloc(ch_d->num_parsers*sizeof(wfunction_t*))))
            return M_OUT_OF_MEM;
        memcpy(ch_d->parsers, ch_s->parsers, ch_d->num_parsers*sizeof(wfunction_t*));
    }
    dest->handler = source->handler;
    dest->flags = source->flags;

    return M_OK;
}

/** 
 * Add a file extension associated with this filetype
 *
 * This function will copy the given ext and add it 
 * to the filetype's list of file extensions.
 **/
M_CODE
lm_filetype_add_extension(filetype_t *ft, const char *name)
{
    if (!ft->e_count)
        ft->extensions = malloc(sizeof(char *));
    else
        ft->extensions = realloc(ft->extensions, (ft->e_count+1)*sizeof(char*));
    if (!ft->extensions)
        return M_OUT_OF_MEM;

    if (!(name = strdup(name)))
        return M_OUT_OF_MEM;
    ft->extensions[ft->e_count] = (char*)name;
    ft->e_count ++;
    return M_OK;
}

/** 
 * Add a mime type associated with this filetype
 **/
M_CODE
lm_filetype_add_mimetype(filetype_t *ft, const char *name)
{
    if (!ft->m_count)
        ft->mimetypes = malloc(sizeof(char *));
    else
        ft->mimetypes = realloc(ft->mimetypes, (ft->m_count+1)*sizeof(char*));
    if (!ft->mimetypes)
        return M_OUT_OF_MEM;

    if (!(name = strdup(name)))
        return M_OUT_OF_MEM;
    ft->mimetypes[ft->m_count] = (char *)name;
    ft->m_count ++;
    return M_OK;
}


/** 
 * Add a parser to this filetype's parser chain
 *
 * TODO: a del_parser function?
 **/
M_CODE
lm_filetype_add_parser(filetype_t *ft, wfunction_t *p)
{
    ft->flags |= FT_FLAG_HAS_PARSER;

    parser_chain_t *ch = &ft->parser_chain;

    if (!(ch->parsers = realloc(ch->parsers, (ch->num_parsers+1)*sizeof(wfunction_t*))))
        return M_OUT_OF_MEM;

    ch->parsers[ch->num_parsers] = p;
    ch->num_parsers++;

    return M_OK;
}

/** 
 * Set this filetypes's list of file extensions.
 **/
M_CODE
lm_filetype_set_extensions(filetype_t *ft, char **extensions, int num_extensions)
{
    int x;

    if (ft->e_count) {
        for (x=0; x<ft->e_count; x++)
            free(ft->extensions[x]);
        free(ft->extensions);
    }

    ft->extensions = extensions;
    ft->e_count    = num_extensions;

    return M_OK;
}

/** 
 * Set this filetypes's list of mimetypes.
 **/
M_CODE
lm_filetype_set_mimetypes(filetype_t *ft, char **mimetypes, int num_mimetypes)
{
    int x;

    if (ft->m_count) {
        for (x=0; x<ft->m_count; x++)
            free(ft->mimetypes[x]);
        free(ft->mimetypes);
    }

    ft->mimetypes = mimetypes;
    ft->m_count   = num_mimetypes;

    return M_OK;
}

/** 
 * Set this filetypes's list of attributes. Attributes are mainly
 * used when connected to a methanol system, where the attributes
 * correspond to columns in the filetype table.
 **/
M_CODE
lm_filetype_set_attributes(filetype_t *ft, char **attributes,
                           int num_attributes)
{
    int x;

    /** 
     * If we have a list of attributes set from before,
     * we must free it since this function *sets* the list,
     * and thus removes any existing entries.
     **/
    if (ft->attr_count) {
        for (x=0; x<ft->attr_count; x++)
            free(ft->attributes[x]);
        free(ft->attributes);
    }

    ft->attributes = attributes; /* take the list directly, don't make a copy of it */
    ft->attr_count = num_attributes;

    return M_OK;
}


/** 
 * Set the UMEX expr for this filetype
 **/
M_CODE
lm_filetype_set_expr(filetype_t *ft, const char *expr)
{
    if (ft->expr)
        umex_free(ft->expr);

    ft->expr = umex_compile(expr);
    return M_OK;
}

