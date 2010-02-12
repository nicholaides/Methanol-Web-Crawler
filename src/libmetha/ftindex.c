/*-
 * ftindex.c
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

#include "url.h"
#include "ftindex.h"
#include "crawler.h"

#include <ctype.h>
#include <string.h>

void 
lm_ftindex_destroy(ftindex_t *i)
{
    int x;
    htpos_t *htp, *prev;

    lm_mimetb_uninit(&i->m_index);

    /* free all entries in the file extension
     * hash table */
    for (x=0; x<16; x++) {
        htp = i->e_index[x];

        while (htp) {
            prev = htp;
            htp = htp->next;
            free(prev);
        }
    }

    if (i->ft_list)
        free(i->ft_list);
}

/** 
 * Generate the index of filetypes, by looking at every filetype's
 * MIME-types and file extensions.
 *
 * NOTE: The list pointed to by ft will be TAKEN (and later freed)
 * by this structure. It must, however, be allocated outside elsewhere.
 **/
M_CODE
lm_ftindex_generate(ftindex_t *i, int count, filetype_t **ft_list)
{
    int x, y, z;

    memset(&i->e_index, 0, sizeof(htpos_t*)*16);
    lm_mimetb_init(&i->m_index);

    for (x=0; x<count; x++) {
        for (y=0; y<ft_list[x]->m_count; y++)
            lm_mimetb_add(&i->m_index, ft_list[x]->mimetypes[y], x);

        for (y=0; y<ft_list[x]->e_count; y++) {
            unsigned int hash = tolower(ft_list[x]->extensions[y][0]);
            for (z=1; z<strlen(ft_list[x]->extensions[y]); z++)
                hash += (hash << 1) ^ tolower(ft_list[x]->extensions[y][z]);
            hash = hash & 0x0F;
            htpos_t *p;
            if (i->e_index[hash]) {
                p = i->e_index[hash];
                while (p->next) {
                    p = p->next;
                }
                p = (p->next = malloc(sizeof(htpos_t)));
            } else
                p = (i->e_index[hash] = malloc(sizeof(htpos_t)));

            p->str = ft_list[x]->extensions[y];
            p->assoc = x;
            p->next = 0;
        }
    }

    i->ft_list  = ft_list;
    i->ft_count = count;
    i->flags    = 0;

    return M_OK;
}

/** 
 * Find a matching filetype by file extensions. Used by lm_ftindex_match_by_url
 *
 * This function also makes sure the umex matches.
 **/
filetype_t *
lm_ftindex_match_by_ext(ftindex_t *i, url_t *url)
{
    unsigned int ehash;
    htpos_t *htp = 0;
    int x;
    char *p;

    ehash = tolower(*(url->str+url->ext_o+1));
    /* generate a file extension hash */
    for (p=url->str+url->ext_o+2; *p && *p != '?'; p++)
        ehash += (ehash << 1) ^ tolower(*p);

    x = p-(url->str+url->ext_o+1);
    htp = i->e_index[ehash & 0x0F];

    while (htp) {
        if (strncasecmp(url->str+url->ext_o+1, htp->str, x) == 0) {
            if (i->ft_list[htp->assoc]->expr) {
                if (umex_match(url, i->ft_list[htp->assoc]->expr))
                    return i->ft_list[htp->assoc];
            } else
                return i->ft_list[htp->assoc];
        }
        htp = htp->next;
    }

    return 0;
}

/**
 * Returns a poitenr to a filetype_t if a filetype was found.
 * If the URL is a possible match, LM_FTINDEX_POSSIBLE_MATCH
 * is returned and a HTTP HEAD must be performed to determine
 * the type. On mismatch, 0 is returned.
 **/
filetype_t *
lm_ftindex_match_by_url(ftindex_t *i, url_t *url)
{
    int x;
    filetype_t *ft;

    if (url->protocol == LM_PROTOCOL_FTP) {
        if (url->file_o == url->sz-1) {
            if (i->flags & LM_FTIFLAG_BIND_FTP_DIR_URL)
                return (filetype_t*)i->ftp_dir_url;
            else
                return 0;
        }
        return lm_ftindex_match_by_ext(i, url);
    }

    if (!url->ext_o) { /* URL does not have a file extension */
        if (LM_URL_ISSET(url, LM_URL_DYNAMIC)) {
            if (i->flags & LM_FTIFLAG_BIND_DYNAMIC_URL) {
                return (filetype_t*)i->dynamic_url;
            } else if (!i->dynamic_url)
                return LM_FTINDEX_POSSIBLE_MATCH;

            for (x=0; x<i->ft_count; x++) {
                if (i->ft_list[x]->expr) {
                    if (umex_match(url, i->ft_list[x]->expr))
                        return i->ft_list[x];
                }
            }

            return 0;
        }

        for (x=0; x<i->ft_count; x++) {
            if (i->ft_list[x]->expr) {
                if (umex_match(url, i->ft_list[x]->expr))
                    return i->ft_list[x];
            }
        }

        if (url->file_o == url->sz-1) {
            /* this URL is a directory only, and the filename is not in the URL */
            if (i->flags & LM_FTIFLAG_BIND_DIR_URL) {
                return (filetype_t*)i->dir_url;
            } else if (!i->dir_url)
                return LM_FTINDEX_POSSIBLE_MATCH;

            return 0;
        }

        if (i->flags & LM_FTIFLAG_BIND_EXTLESS_URL) {
            return (filetype_t*)i->extless_url;
        } else if (!i->extless_url)
            return LM_FTINDEX_POSSIBLE_MATCH;
    } else {
        if (LM_URL_ISSET(url, LM_URL_DYNAMIC)) {
            if (i->flags & LM_FTIFLAG_BIND_DYNAMIC_URL) {
                return (filetype_t*)i->dynamic_url;
            } else if (!i->dynamic_url)
                return LM_FTINDEX_POSSIBLE_MATCH;

            if ((ft = lm_ftindex_match_by_ext(i, url)))
                return ft;

            for (x=0; x<i->ft_count; x++) {
                if (!i->ft_list[x]->e_count && i->ft_list[x]->expr) {
                    if (umex_match(url, i->ft_list[x]->expr))
                        return i->ft_list[x];
                }
            }

            return 0;
        }

        if ((ft = lm_ftindex_match_by_ext(i, url)))
            return ft;

        /* no matching filetype, try finding a filetype by looking comparing the 
         * url to all defined UMEXs */

        for (x=0; x<i->ft_count; x++) {
            if (!i->ft_list[x]->e_count && i->ft_list[x]->expr) {
                if (umex_match(url, i->ft_list[x]->expr))
                    return i->ft_list[x];
            }
        }

        /* this URL has a file extension we have no filetype for */
        if (i->flags & LM_FTIFLAG_BIND_UNKNOWN_URL) {
            return (filetype_t*)i->unknown_url;
        } else if (!i->unknown_url)
            return LM_FTINDEX_POSSIBLE_MATCH;

        return 0;
    }

    return 0;
}

filetype_t *
lm_ftindex_match_by_mime(ftindex_t *i, const char *mime)
{
    FT_ID id;
    if ((id = lm_mimetb_lookup(&i->m_index, mime)) != FT_ID_NULL)
        return i->ft_list[id];
    return 0;
}

