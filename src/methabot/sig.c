/*-
 * sig.c
 * This file is part of Methabot
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

#include <stdlib.h>
#include <signal.h>
#include "methabot.h"

#define VOID static void

VOID sig_int(int s);
VOID sig_seg(int s);

void
sig_register_all(void)
{
    signal(SIGINT, &sig_int);
    signal(SIGSEGV, &sig_seg);
}

VOID
sig_int(int s)
{
    printf("\r-- SIGINT, exiting\n");
    exit(0);
    /*static int force = 0;
    if (!force) {
        force = 1;
        printf("\n-- SIGINT received, finishing transfers\n");
        printf("-- hit Ctrl+C again to force quit\n");
    } else {
        printf("\n-- SIGINT received (2), cleaning up ...\n");
        printf("-- closing connections ...         ");
        printf("done\n");
        printf("-- flushing files ...              ");
        printf("done\n");
        exit(0);
    }*/
}

VOID
sig_seg(int s)
{
    printf("\r-- SIGSEGV, exiting\n");
    printf("-- segmentation fault\n");
    exit(EXIT_FAILURE);
}
