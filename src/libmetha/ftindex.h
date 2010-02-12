/*-
 * ftindex.h
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

/** 
 * Filetype index
 *
 * Used by libmetha for relating and looking up urls, to 
 * determine what kind of filetype the URL points to. 
 **/

#ifndef _FTINDEX__H_
#define _FTINDEX__H_

#include "mime.h"
#include "filetype.h"

#define LM_FTINDEX_POSSIBLE_MATCH ((filetype_t*)-1)

enum {
    LM_FTIFLAG_BIND_DYNAMIC_URL = 1,
    LM_FTIFLAG_BIND_EXTLESS_URL = 1<<1,
    LM_FTIFLAG_BIND_DIR_URL     = 1<<2,
    LM_FTIFLAG_BIND_UNKNOWN_URL = 1<<3,
    LM_FTIFLAG_BIND_FTP_DIR_URL = 1<<4,
};

/* a position in the file extensions hash table */
typedef struct htpos {
    const char *str;
    uint8_t     assoc;
    struct htpos *next;
} htpos_t;

typedef struct ftindex {
    mimetb_t m_index;
    htpos_t *e_index[16];
    
    /**
     * These following five are set to strings by lmetha_load_config() when a 
     * configuration file is a loaded. The strings can be either "lookup", 
     * "discard" or a '@'-sign followed by a filetype name.
     *
     * When lmetha_prepare() is run, these values will be replaced. I'll
     * list what happens if and when:
     * - If the option is set to "discard", set the value to 1
     * - If the option is set to "lookup", set the value to 0
     * - If the option is set to a filetype, set the value to a pointer
     *   to the corresponding filetype, and set the corresponding 
     *   flag (see LM_FTIFLAG_BIND_*) in crawler_t->flags.
     *
     * At execution time, workers will check how to act by first lookng
     * at the flag. If, for instance, the flag LM_FTIFLAG_BIND_DYNAMIC_URL
     * is set, then the worker will know that dynamic_url will be set 
     * to a pointer to the filetype to bind the URL with.
     **/
    void       *dynamic_url;
    void       *extless_url;
    void       *dir_url;
    void       *unknown_url;
    void       *ftp_dir_url;

    uint8_t    flags;

    filetype_t **ft_list;
    int          ft_count;
} ftindex_t;

M_CODE      lm_ftindex_generate(ftindex_t *i, int count, filetype_t **ft_list);
filetype_t *lm_ftindex_match_by_url(ftindex_t *i, url_t *url);
filetype_t *lm_ftindex_match_by_ext(ftindex_t *i, url_t *url);
filetype_t *lm_ftindex_match_by_mime(ftindex_t *i, const char *mime);
void        lm_ftindex_destroy(ftindex_t *i);

#endif

