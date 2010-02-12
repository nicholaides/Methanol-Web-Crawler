/*-
 * dump.c
 * This file is part of methabot 
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
 * This file contains some UGLY dumping functions primarily
 * intended for developers.
 **/

#include <stdio.h>
#include "../libmetha/metha.h"
#include "../libmetha/crawler.h"
#include "../libmetha/filetype.h"
#include "../libmetha/ftindex.h"

void
mb_dump_filetypes(metha_t *m)
{
    int x, y;
    for (x=0; x<m->num_filetypes; x++) {
        fprintf(stderr, "# DUMP of filetype '%s'\n", m->filetypes[x]->name);

        if (m->filetypes[x]->e_count) {
            fprintf(stderr, "  file extensions:\n    ");
            for (y=0; y<m->filetypes[x]->e_count; y++)
                fprintf(stderr, "%s ", m->filetypes[x]->extensions[y]);
            fprintf(stderr, "\n");
        }

        if (FT_FLAG_ISSET(m->filetypes[x], FT_FLAG_HAS_PARSER))
            fprintf(stderr, "  parser: '%s'\n", m->filetypes[x]->parser_str);

        fprintf(stderr, "# END\n");
    }
}

void
mb_dump_crawlers(metha_t *m)
{
    int x, y;
    for (x=0; x<m->num_crawlers; x++) {
        fprintf(stderr, "# DUMP of crawler '%s'\n", m->crawlers[x]->name);

        if (m->crawlers[x]->num_filetypes) {
            fprintf(stderr, "  filetypes:\n    ");
            for (y=0; y<m->crawlers[x]->num_filetypes; y++)
                fprintf(stderr, "%s ", m->crawlers[x]->filetypes[y]);
            fprintf(stderr, "\n");
        }

        if (m->crawlers[x]->ftindex.flags & LM_FTIFLAG_BIND_DYNAMIC_URL) {
            fprintf(stderr, "  dynamic_url = bind(id = %hhd, name = %s)\n",
                    ((filetype_t*)m->crawlers[x]->ftindex.dynamic_url)->id,
                    ((filetype_t*)m->crawlers[x]->ftindex.dynamic_url)->name);
        } else {
            if (m->crawlers[x]->ftindex.dynamic_url)
                fprintf(stderr, "  dynamic_url = discard\n");
            else
                fprintf(stderr, "  dynamic_url = lookup\n");
        }
        if (m->crawlers[x]->ftindex.flags & LM_FTIFLAG_BIND_EXTLESS_URL) {
            fprintf(stderr, "  extless_url = bind(id = %hhd, name = %s)\n",
                    ((filetype_t*)m->crawlers[x]->ftindex.extless_url)->id,
                    ((filetype_t*)m->crawlers[x]->ftindex.extless_url)->name);
        } else {
            if (m->crawlers[x]->ftindex.extless_url)
                fprintf(stderr, "  extless_url = discard\n");
            else
                fprintf(stderr, "  extless_url = lookup\n");
        }
        if (m->crawlers[x]->ftindex.flags & LM_FTIFLAG_BIND_DIR_URL) {
            fprintf(stderr, "  dir_url     = bind(id = %hhd, name = %s)\n",
                    ((filetype_t*)m->crawlers[x]->ftindex.dir_url)->id,
                    ((filetype_t*)m->crawlers[x]->ftindex.dir_url)->name);
        } else {
            if (m->crawlers[x]->ftindex.dir_url)
                fprintf(stderr, "  dir_url     = discard\n");
            else
                fprintf(stderr, "  dir_url     = lookup\n");
        }
        if (m->crawlers[x]->ftindex.flags & LM_FTIFLAG_BIND_UNKNOWN_URL) {
            fprintf(stderr, "  unknown_url = bind(id = %hhd, name = %s)\n",
                    ((filetype_t*)m->crawlers[x]->ftindex.unknown_url)->id,
                    ((filetype_t*)m->crawlers[x]->ftindex.unknown_url)->name);
        } else {
            if (m->crawlers[x]->ftindex.unknown_url)
                fprintf(stderr, "  unknown_url = discard\n");
            else
                fprintf(stderr, "  unknown_url = lookup\n");
        }

        fprintf(stderr, "# END\n");
    }
}
