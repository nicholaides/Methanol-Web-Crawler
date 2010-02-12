/*-
 * daemon.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>

#include "lmc.h"
#include "daemon.h"

#define CHILD_SUCCESS 0
#define CHILD_FAILURE 1

/** 
 * Function used by mn-masterd, mn-slaved and mb-client.
 *
 * Fork and load the configuration file specified in
 * 'config'. 'user' and 'group' points to strings 
 * identifying the names of a user and group to 
 * setuid and setgid to. setuid is done AFTER the 
 * loading of the configuration file, so the configuration
 * file may alter the strings.
 *
 * A pipe will be opened to the forked process, this pipe
 * is used to notify failure or success.
 *
 * Once the fork and loading of the configuration 
 * is completed, 'run' will be called to start the
 * daemon.
 *
 * Return 0 if the daemon was successfully started.
 **/
int
nol_daemon_launch(const char *config,
                  lmc_parser_t *lmc,
                  const char **user,
                  const char **group, 
                  const char *(*init_cb)(void),
                  const char *(*run_cb)(void),
                  int dofork)
{
    pid_t pid;
    const char *err;
    int   fds[2];
    char  buf[257];
    int   n;
    char  c;
    struct group  *g;
    struct passwd *p;
    M_CODE m;

    if (dofork) {
        /* set up the pipe so we can communicate with
         * the child process, and keep track of whether
         * it succeeds or not */
        if (pipe(fds) == -1) {
            fprintf(stderr, "pipe() failed\n");
            return 1;
        }
        if ((pid = fork()) == -1) {
            close(fds[1]);
            close(fds[0]);
            fprintf(stderr, "fork() failed\n");
            return 1;
        }
    } else
        pid = 0;
    if (pid == 0) { /* if we are the child or dofork was set to 0 */
        /*close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);*/
        if (dofork)
            close(fds[0]);

        do {
            /* now we'll configure and write any
             * error message to the pipe */
            if ((m = lmc_parse_file(lmc, config)) != M_OK) {
                if (!(err = lmc->last_error)) {
                    snprintf(buf, 255, "error %d while parsing configuration file", m);
                    err = buf;
                }
                break;
            }
            if ((err = init_cb()) != 0)
                break;
            if (group && *group) {
                if (!(g = getgrnam(*group))) {
                    snprintf(buf, 255, "could not get GID for '%s'", *group);
                    err = buf;
                    break;
                }
                if (setgid(g->gr_gid) != 0) {
                    snprintf(buf, 255, "could not change GID: %s",
                            strerror(errno));
                    err = buf;
                    break;
                }
            }
            if (user && *user) {
                if (!(p = getpwnam(*user))) {
                    snprintf(buf, 255, "could not get UID for '%s'", *user);
                    err = buf;
                    break;
                }
                if (setuid(p->pw_uid) != 0) {
                    snprintf(buf, 255, "could not change UID: %s",
                            strerror(errno));
                    err = buf;
                    break;
                }
            }
            if (dofork) {
                c = CHILD_SUCCESS;
                write(fds[1], &c, 1);
                close(fds[1]);
            }

            run_cb();
            exit(0);
        } while (0);

        /* reach here on error */
        n = strlen(err);
        if (dofork) {
            if (n > 255) n = 255;
            c = CHILD_FAILURE;
            write(fds[1], &c, 1);
            write(fds[1], err, n);
            close(fds[1]);
        } else 
            fprintf(stderr, "failed: %s\n", err);
        exit(1);
    }

    /* child doesn't reach here */
    close(fds[1]);
    if ((n = read(fds[0], &buf, 1)) <= 0)
        fprintf(stderr, "failed: no data from child, possible crash?\n");
    else if (*buf == CHILD_SUCCESS) {
        close(fds[0]);
        return 0;
    } else if (n == 1) {
        if ((n = read(fds[0], &buf, 255)) <= 0)
            fprintf(stderr, "failed: unknown error in child\n");
        else {
            buf[n] = '\0';
            fprintf(stderr, "failed: %s\n", buf);
        }
    }

    close(fds[0]);
    return 1;
}

