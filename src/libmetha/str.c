/*-
 * str.c
 * This file is part of libmetha
 *
 * Copyright (C) 2008, Emil Romanus <emil.romanus@gmail.com>
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

#include "str.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/** 
 * This function does not actually convert the string to
 * a standards-compliant URL, but it only adds a protocol
 * infront of the given URL, guessing which protocol 
 * depending on how the string looks. Expected input is 
 * something like "www.google.com/x/" or "ftp.x.com".
 **/
char *
lm_strtourl(const char* str)
{
    int extra_size;
    const char *prefix;
    char *out;
    char *s = (char*)str;

    while (isalnum(*s))
        s++;

    if (*s == ':')
        return strdup(str); /* protocol is already set */

    if (*str == '/' || (*str == '.' && *(str+1) == '/')) {
        extra_size = 7;
        prefix = "file://";
    } else if (strncasecmp(str, "ftp.", 4) == 0) {
        extra_size = 6;
        prefix = "ftp://";
    } else {
        extra_size = 7;
        prefix = "http://";
    }

    if (!(out = malloc(strlen(str)+1+extra_size)))
        return 0;

    sprintf(out, "%s%s", prefix, str);

    return out;
}

