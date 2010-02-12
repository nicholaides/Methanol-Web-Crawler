/*-
 * hook.c
 * This file is part of Methanol
 *
 * Copyright (c) 2009, Emil Romanus <sdac@bithack.se>
 * http://metha-sys.org/
 * http://bithack.se/projects/methabot/
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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "hook.h"

/* keep in sync with the enum in hook.h */
static
const char *hook_str[] =
{
    "session-complete",
    "cleanup",
};

/* set to 1 or 0 whether the hook
 * has been saved */
static int hook_list[NUM_HOOKS];

/* save a script as a hook */
int
nol_s_hook_assign(const char *hook_nm,
                const char *code,
                size_t code_len)
{
    int  x;
    int  valid = 0;
    FILE *fp;

    for (x=0; x<NUM_HOOKS; x++) {
        if (strcmp(hook_str[x], hook_nm) == 0) {
            valid = 1;
            break;
        }
    }
    if (!valid) {
        syslog(LOG_ERR, "unknown hook type '%s'\n", hook_nm);
        return -1;
    }
    /* we should have chdir'ed to exec_dir already */
    if (!(fp = fopen(hook_nm, "wb"))) {
        syslog(LOG_ERR, "failed to open hook file '%s': %s", 
                hook_nm,
                strerror(errno));
        return -1;
    }

    fwrite(code, 1, code_len, fp);
    fclose(fp);
    chmod(hook_nm, S_IXUSR | S_IWUSR | S_IRUSR);

    hook_list[x] = 1;

    syslog(LOG_INFO, "registered hook '%s'\n", hook_nm);
    return 0;
}

/** 
 * start the hook script
 *
 * Currently this function simply runs system(),
 * but we should fork and do an exec() instead
 **/
int
nol_s_hook_invoke(unsigned hook_id)
{
    char    *run;
    if (hook_id >= NUM_HOOKS || !hook_list[hook_id])
        return -1;
#ifdef DEBUG
    syslog(LOG_DEBUG, "invoking hook '%s'\n", hook_str[hook_id]);
#endif
    /*
    asprintf(&run, "./%s %d",
            hook_str[hook_id], sess_id);
            */
    asprintf(&run, "./%s",
            hook_str[hook_id]);
    system(run);
    free(run);
    return 0;
}

/* remove all created files, this is not critical but its always
 * good to cleanup! */
void
nol_s_hook_cleanup_all(void)
{
    int x;

    for (x=0; x<NUM_HOOKS; x++) {
        if (hook_list[x]) {
            unlink(hook_str[x]);
            hook_list[x] = 0;
        }
    }
}

