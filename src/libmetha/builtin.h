/*-
 * builtin.h
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

#ifndef _LM_BUILTIN__H_
#define _LM_BUILTIN__H_

#include <stdlib.h>

/** 
 * Builtin parsers and URL extraction functions
 **/

/* html.c */
M_CODE lm_parser_html(struct worker *w, struct iobuf *buf, struct uehandle *ue_h, struct url *url, struct attr_list *al);
M_CODE lm_parser_xmlconv(struct worker *w, struct iobuf *buf, struct uehandle *ue_h, struct url *url, struct attr_list *al);

/* builtin.c */
M_CODE lm_parser_css(struct worker *w, struct iobuf *buf, struct uehandle *ue_h, struct url *url, struct attr_list *al);
M_CODE lm_parser_ftp(struct worker *w, struct iobuf *buf, struct uehandle *ue_h, struct url *url, struct attr_list *al);
M_CODE lm_parser_text(struct worker *w, struct iobuf *buf, struct uehandle *ue_h, struct url *url, struct attr_list *al);
M_CODE lm_extract_css_urls(struct uehandle *ue_h, char *p, size_t sz);
M_CODE lm_extract_text_urls(struct uehandle *ue_h, char *p, size_t sz);
M_CODE lm_handler_writefile(worker_t *w, iohandle_t *io, url_t *url);

#endif
