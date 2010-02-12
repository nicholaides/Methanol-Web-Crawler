/*-
 * main.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../libmetha/metha.h"
#include "../libmetha/str.h"
#include "config.h"
#include "methabot.h"

static metha_t     *m;

/* command line options, with default values */
static unsigned int num_pipelines       = 8;
static unsigned int num_threads         = 1;
static int          dump                = 0;
static int          silent              = 0;
static int          io_verbose          = 0;
static int          download            = 0;
static char        *download_dir        = 0;
static int          external            = 0;
static unsigned int external_peek       = 0;
static unsigned int depth_limit         = (unsigned int)1;
static char        *user_agent          = "Methabot/" VERSION;
static int          no_duplicates       = 0;
static char        *proxy               = 0;
static char        *mode                = 0;
static int          cookies             = 0;
static int          jail                = 0;
static int          robotstxt           = 0;
static char        *extless_url         = 0;
static char        *dynamic_url         = 0;
static char        *unknown_url         = 0;
static char        *dir_url             = 0;
static char        *base_url            = 0;
static char        *extensions          = 0;
static char        *mimetypes           = 0;
static char        *crawler             = 0;
static char        *filetype            = 0;
static char        *expr                = 0;
static char        *parser              = 0;
static char        *type                = 0;
static int          spread_workers      = 0;
static char        *config              = 0;
static char        *handler             = 0;
static char        *def_handler         = 0;

/* methabot-specific data */
char        *home_conf           = 0; /* user-specific configuration directory */
int          home_conf_sz        = 0; /* length of above */
char        *install_conf        = 0; /* system-wide configuration direcotry */
int          install_conf_sz     = 0;
char        *home_scripts        = 0; /* user-specific script directory */
int          home_scripts_sz     = 0; /* length of above */
char        *install_scripts     = 0; /* system-wide scripts direcotry */
int          install_scripts_sz  = 0;

static struct option opts[] =
{
    {"silent",          no_argument,        0,      's'},
    {"num-pipelines",   required_argument,  0,      'N'},
    {"num-workers",     required_argument,  0,      'n'},
    {"base-url",        required_argument,  0,      'b'},
    {"user-agent",      required_argument,  0,      'a'},
    {"external-peek",   required_argument,  0,      'p'},
    {"extensions",      required_argument,  0,      't'},
    {"mimetypes",       required_argument,  0,      'm'},
    {"expr",            required_argument,  0,      'x'},
    {"external",        no_argument,        0,      'e'},
    {"depth-limit",     required_argument,  0,      'D'},
    {"io-verbose",      no_argument,        0,      0},
    {"download",        no_argument,        0,      'd'},
    {"no-duplicates",   no_argument,        0,      0},
    {"proxy",           required_argument,  0,      0},
    {"mode",            required_argument,  0,      'M'},
    {"enable-cookies",  no_argument,        0,      'c'},
    {"version",         no_argument,        0,      'v'},
    {"robotstxt",       no_argument,        0,      'r'},
    {"extless-url",     required_argument,  0,      1},
    {"dynamic-url",     required_argument,  0,      2},
    {"unknown-url",     required_argument,  0,      3},
    {"dir-url",         required_argument,  0,      4},
    {"crawler",         required_argument,  0,      5},
    {"filetype",        required_argument,  0,      6},
    {"parser",          required_argument,  0,      7},
    {"spread-workers",  no_argument,        0,      8},
    {"type",            required_argument,  0,      'T'},
    {"config",          required_argument,  0,      9},
    {"jail",            no_argument,        0,      'j'},
    {"handler",         required_argument,  0,      10},
    {"default-handler", required_argument,  0,      11},
    {0, 0, 0, 0}
};

/* dump.c */
void mb_dump_filetypes(metha_t *m);
void mb_dump_crawlers(metha_t *m);

/* help.c */
void mb_option_help(const char *s);
void mb_help(void);
void mb_license(void);
void mb_version(void);
void mb_examples(void);
void mb_info(void);

int  mb_init(void);
void mb_uninit(void);
static M_CODE mb_configure_filetype(void);
static M_CODE mb_configure_crawler(void);
M_CODE mb_downloader_cb(void *private, const url_t *url);
static void mb_warning_cb(metha_t *m, const char *s, ...);
static void mb_error_cb(metha_t *m, const char *s, ...);
static void mb_status_cb(metha_t *m, struct worker *w, url_t *url);
static void mb_target_cb(metha_t *m, struct worker *w, url_t *url, attr_list_t *attributes, filetype_t *ft);
static void mb_status_silent_cb(metha_t *m, struct worker *w, url_t *url);

extern char *optarg;
extern int   opterr;
extern int   optind;

int
main(int argc, char **argv)
{
    M_CODE   status;
    int      c, x;
    int      opts_index = 0;
    int      num_conf = 0;
    int      from_stdin = 0;

    sig_register_all();

    if (!mb_init())
        return 1;

    if (argc < 2) {
        fprintf(stderr, "mb: nothing to do, try `mb --help' for more information\n");
        mb_uninit();
        return 0;
    } else if (strncmp(argv[1], "--", 2) == 0) {
        do {
            if (strcmp(argv[1]+2, "help") == 0) {
                if (argc > 2)
                    mb_option_help(argv[2]);
                else
                    mb_help();
            }
            else if (strcmp(argv[1]+2, "info") == 0)
                mb_info();
            else if (strcmp(argv[1]+2, "license") == 0)
                mb_license();
            else if (strcmp(argv[1]+2, "examples") == 0)
                mb_examples();
            else
                break;

            return 0;
        } while (0);
    }

    do {
        opts_index = 0;
        c = getopt_long(argc, argv, "da:vsb:n:N:%:D:p:em:t:x:C:M:cT:jr", opts, &opts_index);

        if (c == -1)
            break;

        switch (c) {
            case 0:
                if (strcmp(opts[opts_index].name, "io-verbose") == 0)
                    io_verbose = 1;
                else if (strcmp(opts[opts_index].name, "no-duplicates") == 0)
                    no_duplicates = 1;
                else if (strcmp(opts[opts_index].name, "proxy") == 0)
                    proxy = optarg;
                break;

            case 1:   extless_url    = optarg; break;
            case 2:   dynamic_url    = optarg; break;
            case 3:   unknown_url    = optarg; break;
            case 4:   dir_url        = optarg; break;
            case 5:   crawler        = optarg; break;
            case 6:   filetype       = optarg; break;
            case 7:   parser         = optarg; break;
            case 9:   config         = optarg; break;
            case 10:  handler        = optarg; break;
            case 11:  def_handler    = optarg; break;
            case 'a': user_agent     = optarg; break;
            case 'b': base_url       = optarg; break;
            case 'm': mimetypes      = optarg; break;
            case 't': extensions     = optarg; break;
            case 'x': expr           = optarg; break;
            case 'C': download_dir   = optarg; break;
            case 'M': mode           = optarg; break;
            case 'T': type           = optarg; break; 
            case 'N': num_pipelines  = (unsigned int)atoi(optarg); break; 
            case 'n': num_threads    = (unsigned int)atoi(optarg); break; 
            case '%': dump           = (unsigned int)atoi(optarg); break; 
            case 'p': external_peek  = (unsigned int)atoi(optarg); break; 
            case 'D': depth_limit    = (unsigned int)atoi(optarg); break;
            case 'e': external       = 1; break;
            case 'd': download       = 1; break; 
            case 'c': cookies        = 1; break;
            case 's': silent         = 1; break; 
            case 'j': jail           = 1; break; 
            case 'r': robotstxt      = 1; break; 
            case 8:   spread_workers = 1; break;

            case 'v': mb_version(); exit(0);

            case '?':
                /*fprintf(stderr, "mb: error: unknown option --%s\n", opts[opts_index].name);*/
                exit(1);

            default:
                /*fprintf(stderr, "mb: error: unknown option -%c\n", c);*/
                exit(1);
        }
    } while (1);

    argc -= optind;
    argv += optind;

    if (lmetha_global_init() != M_OK) {
        fprintf(stderr, "global initialization failed\n");
        goto error;
    }

    if (!(m = lmetha_create()))
        goto outofmem;

    if (silent)
        lmetha_setopt(m, LMOPT_STATUS_FUNCTION, &mb_status_silent_cb);
    else
        lmetha_setopt(m, LMOPT_STATUS_FUNCTION, &mb_status_cb);
    lmetha_setopt(m, LMOPT_ERROR_FUNCTION, &mb_error_cb);
    lmetha_setopt(m, LMOPT_TARGET_FUNCTION, &mb_target_cb);
    lmetha_setopt(m, LMOPT_WARNING_FUNCTION, &mb_warning_cb);


    /* set up the default configuration file and script directories */

    if ((status = lmetha_setopt(m, LMOPT_PRIMARY_SCRIPT_DIR, home_scripts)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_SECONDARY_SCRIPT_DIR, install_scripts)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_PRIMARY_CONF_DIR, home_conf)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_SECONDARY_CONF_DIR, install_conf)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_MODULE_DIR, METHA_MODULE_DIR)) != M_OK)
        goto error;

    /* if the user specified which configuration file to use, load it/them */
    for (x=0; x<argc; x++) {
        if (*argv[x] == ':') {
            char name[64];

            if (snprintf(name, 64, "%s.conf", argv[x]+1) < 64 &&
                (status = lmetha_load_config(m, name)) != M_OK) {
                switch (status) {
                    case M_COULD_NOT_OPEN:
                        fprintf(stderr, "mb: error: unable not load %s\n", name);
                        break;
                    default:
                        fprintf(stderr, "mb: error: error while loading %s\n", name);
                }
                goto error;
            }
            num_conf ++;
        } else if (strcmp(argv[x], "-") == 0)
            from_stdin = 1;
        else
            continue;

        /* reorder the array, we need all urls by themselves in a list
         * later, so we must remove all configurations */
        argv[x] = argv[argc-1];
        argc--;
        x--;
    }
    if (!config) {
        if (!num_conf) {
            /* by default, load default.conf */
            if ((status = lmetha_load_config(m, "default.conf")) != M_OK) {
                fprintf(stderr, "mb: error: unable not load default.conf\n");
                goto error;
            }
        }
    } else {
        if (*config == '/')
            status = lmetha_load_config(m, config);
        else {
            char *name = malloc(256); /* TODO: don't restrict the length to 256 chars */
            char *wd = getcwd(0, 0);
            if (name && wd) {
                snprintf(name, 256, "%s/%s", wd, config);
                status = lmetha_load_config(m, name);
                free(name);
                free(wd);
            } else goto outofmem;
        }
        if (status != M_OK) {
            fprintf(stderr, "mb: error: unable not load %s\n", config);
            goto error;
        }
    }

    /* TODO: --download */
    /*
    if (download) {
        if ((status = lmetha_setopt(m, LMOPT_FILE_HANDLER, &mb_downloader_cb)) != M_OK)
            goto error;
    }
    */

    if (mode) {
        if ((status = lmetha_setopt(m, LMOPT_MODE, mode)) != M_OK)
            goto error;
    }

    if ((status = lmetha_setopt(m, LMOPT_ENABLE_COOKIES, cookies)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_ENABLE_BUILTIN_PARSERS, 1)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_IO_VERBOSE, io_verbose)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_NUM_THREADS, num_threads)) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_INITIAL_CRAWLER, "default")) != M_OK)
        goto error;
    if ((status = lmetha_setopt(m, LMOPT_USERAGENT, user_agent)) != M_OK)
        goto error;

    if (proxy) {
        if ((status = lmetha_setopt(m, LMOPT_PROXY, proxy)) != M_OK)
            goto error;
    }

    if (mb_configure_filetype() != M_OK)
        goto error;
    if (mb_configure_crawler() != M_OK)
        goto error;

    if ((status = lmetha_prepare(m)) != M_OK)
        goto error;

    if (dump >= 1) {
        mb_dump_filetypes(m);
        mb_dump_crawlers(m);
    }

    if (download_dir)
        chdir(download_dir);

    if (from_stdin) {
        if (!base_url) {
            fprintf(stderr, "mb: error: reading from stdin requires a base url\n");
            goto error;
        }
        /* read from stdin */
        char  *stdin_buf = malloc(1024);
        char  *p = stdin_buf;
        size_t stdin_bufsz = 0;
        size_t stdin_bufcap = 1024;
        size_t r;

        while ((r = fread(p, 1, 1024, stdin)) == 1024) {
            stdin_bufcap += 1024;
            stdin_buf = realloc(stdin_buf, stdin_bufcap);
            p = stdin_buf + stdin_bufcap-1024;
        }
        stdin_bufsz = (p+r)-stdin_buf;

        if (!(p = lm_strtourl(base_url)))
            goto outofmem;

        lmetha_exec_provided(m, p, stdin_buf, stdin_bufsz);

        free(p);
        free(stdin_buf);
    }

    if ((status = lmetha_exec(m, argc, (const char **)argv)) != M_OK)
        goto error;

    lmetha_destroy(m);
    lmetha_global_cleanup();
    mb_uninit();
    return 0;

error:
    if (m)
        lmetha_destroy(m);
    lmetha_global_cleanup();
    mb_uninit();
    return 1;

outofmem:
    fprintf(stderr, "mb: error: out of memory\n");
    return 1;
}

/** 
 * Configure the filetype
 **/
static M_CODE
mb_configure_filetype(void)
{
    filetype_t *ft;
    char       *p, *prev;

    if (!filetype) {
        /* no filetype name specified, create a new filetype named "custom" */
        ft = lm_filetype_create("custom", 6);

        if (lmetha_add_filetype(m, ft) != M_OK)
            return M_FAILED;
    } else {
        if (!(ft = lmetha_get_filetype(m, filetype))) {
            fprintf(stderr, "mb: error: filetype \"%s\" not found\n", filetype);
            return M_FAILED;
        }
    }

    if (mimetypes) {
        lm_filetype_set_mimetypes(ft, 0, 0);
        p = mimetypes;
        prev = p;
        while ((p = strchr(p, ','))) {
            *p = '\0';
            p++;
            lm_filetype_add_mimetype(ft, prev);
            prev = p;
        }
        lm_filetype_add_mimetype(ft, prev);
    }

    if (extensions) {
        lm_filetype_set_extensions(ft, 0, 0);
        p = extensions;
        prev = p;
        while ((p = strchr(p, ','))) {
            *p = '\0';
            p++;
            lm_filetype_add_extension(ft, prev);
            prev = p;
        }
        lm_filetype_add_extension(ft, prev);
    }

    if (expr)
        lm_filetype_set_expr(ft, expr);
    if (parser)
        ft->parser_str = strdup(parser);
    if (handler)
        ft->handler.name = strdup(handler);

    return M_OK;
}

/** 
 * Configure the crawler
 **/
static M_CODE
mb_configure_crawler(void)
{
    crawler_t *cr;

    if (!crawler)
        crawler = "default";
    if (!(cr = lmetha_get_crawler(m, crawler))) {
        fprintf(stderr, "mb: error: crawler \"%s\" not found\n", crawler);
        return M_FAILED;
    }
    if (dynamic_url)
        cr->ftindex.dynamic_url = strdup(dynamic_url);
    if (extless_url)
        cr->ftindex.extless_url = strdup(extless_url);
    if (dir_url)
        cr->ftindex.dir_url = strdup(dir_url);
    if (unknown_url)
        cr->ftindex.unknown_url = strdup(unknown_url);
    if (external)
        lm_crawler_flag_set(cr, LM_CRFLAG_EXTERNAL);
    if (spread_workers)
        lm_crawler_flag_set(cr, LM_CRFLAG_SPREAD_WORKERS);
    if (jail)
        lm_crawler_flag_set(cr, LM_CRFLAG_JAIL);
    if (robotstxt)
        lm_crawler_flag_set(cr, LM_CRFLAG_ROBOTSTXT);
    if  (def_handler) {
        cr->default_handler.name = strdup(def_handler);
    }

    cr->peek_limit = external_peek;
    cr->depth_limit = depth_limit;

    if (type)
        cr->initial_filetype.name = strdup(type);

    return M_OK;
}


/** 
 * Called by libmetha whenever a filetype without a parser has 
 * been matched, this function is only used when the user 
 * has enabled the download flag
 **/
M_CODE
mb_downloader_cb(void *private, const url_t *url)
{
    char *name;
    char *ext;
    int  ext_offs;
    int  x;
    struct stat st;
    CURL *h = curl_easy_init();

    if (!h)
        return M_OUT_OF_MEM;

    /** 
     * Get the filename
     **/
    if (url->ext_o) {
        for (x = url->ext_o; *(url->str+x) && *(url->str+x) != '?'; x++)
            ;

        if (!(ext = malloc(x-url->ext_o+1)))
            return M_OUT_OF_MEM;

        memcpy(ext, url->str+url->ext_o, x-url->ext_o);
        ext[x-url->ext_o] = '\0';

        ext_offs = url->ext_o-(url->file_o+1);
    } else {
        ext = strdup("");
        for (x = url->file_o+1; *(url->str+x) && *(url->str+x) != '?'; x++)
            ;
        ext_offs = x-(url->file_o+1);
    }

    if (url->file_o+1 == url->sz) {
        if (!(name = malloc(strlen("index.html")+1+32)))
            return M_OUT_OF_MEM;
        memcpy(name, url->str+url->file_o+1, ext_offs);
        ext_offs = strlen("index");
        ext = strdup(".html");
    } else {
        if (!(name = malloc(ext_offs+strlen(ext)+1+32)))
            return M_OUT_OF_MEM;

        memcpy(name, url->str+url->file_o+1, ext_offs);
        strcpy(name+ext_offs, ext);
    }

    int d=1;
    x=0;
    if (stat(name, &st) == 0) {
        if(no_duplicates) {
            d = 0;
        } else {
            do {
                x++;
                name[ext_offs] = '\0';

                sprintf(name, "%s-%d%s", name, x, ext);
            } while (stat(name, &st) == 0);
        }
    }

    if(d) {
        FILE *fp = fopen(name, "w+b");
        if (fp) {
            printf("[v] %s\n", name);
            curl_easy_setopt(h, CURLOPT_URL, url->str);
            if(proxy)
                curl_easy_setopt(h, CURLOPT_PROXY, proxy);
            curl_easy_setopt(h, CURLOPT_WRITEDATA, fp);
            int ret = curl_easy_perform(h);
            if(ret >= 5 && ret <= 7) {
                printf("could not download because of the proxy\n");
            }
        } else {
            /*LM_ERROR(m, "download of '%s' failed\n", name);*/
        }
    }

    free(name);
    free(ext);
    curl_easy_cleanup(h);

    return M_OK;
}

/** 
 * Set up some methabot-specific information...
 *
 * See if the user running methabot has got a ~/.methabot/ 
 * directory. If not, create it. We use that directory for 
 * saving caches of compiled MCL-scripts and stop-continue 
 * status information for later.
 *
 * The function stores the user-specific configuration 
 * directory in in home_conf, and the system-wide in 
 * install_conf.
 *
 * Returns 0 on fail.
 **/
int
mb_init(void)
{
    char *mbdir,
         *p;
    int   p_len;

    install_conf       = strdup(METHA_CONFIG_DIR);
    install_conf_sz    = strlen(METHA_CONFIG_DIR);
    install_scripts    = strdup(METHA_SCRIPT_DIR);
    install_scripts_sz = strlen(METHA_SCRIPT_DIR);

    /* the user can override his script and conf directories using the
     * MB_CONF_PATH and MB_SCRIPT_PATH env variables */

    if ((p = getenv("MB_SCRIPT_PATH"))) {
        home_scripts_sz = strlen(p);
        home_scripts = strdup(p);
    }
    if ((p = getenv("MB_CONF_PATH"))) {
        home_conf_sz = strlen(p);
        home_conf = strdup(p);
    }

    if (!home_conf_sz || !home_scripts_sz) {
        if (!(p = getenv("HOME")))
            fprintf(stderr, "mb: warning: couldn't get home directory from env\n");
            /* even though we couldn't get the users home directory, we are still
             * able to look through the system-wide config */
        else {
            if (!(mbdir = malloc(strlen(p)+1+strlen("/.methabot"))))
                return 0;

            p_len = strlen(p);
            memcpy(mbdir, p, p_len);
            strcpy(mbdir+p_len, "/.methabot");
            if (mkdir(mbdir, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
                if (errno != EEXIST)
                    fprintf(stderr,
                            "mb: warning: could not create %s/ [%d: %s]\n",
                            mbdir, errno, strerror(errno));
            } else {
                fprintf(stderr, "mb: created %s/ with permissions 700\n", mbdir);
            }

            p_len += strlen("/.methabot");

            if (!home_conf_sz) {
                home_conf = mbdir;
                home_conf_sz = p_len;
            }

            if (!home_scripts_sz) {
                home_scripts_sz = p_len+strlen("/scripts");
                if (!(home_scripts = malloc(home_scripts_sz+1))) {
                    fprintf(stderr, "mb: out of mem\n");
                    abort();
                }
                memcpy(home_scripts, mbdir, p_len);
                strcpy(home_scripts+p_len, "/scripts");

                if (mkdir(home_scripts, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
                    if (errno != EEXIST)
                        fprintf(stderr,
                                "mb: warning: could not create %s/ [%d: %s]\n",
                                home_scripts, errno, strerror(errno));
                } else {
                    fprintf(stderr, "mb: created %s/ with permissions 700\n",
                            home_scripts);
                    fprintf(stderr,
                            "mb: for security reasons, you should not"
                            "change these permissions\n");
                }
            }
        }
    }

    return 1;
}

static void
mb_status_silent_cb(metha_t *m, struct worker *w, url_t *url)
{
    return;
}

static void
mb_target_cb(metha_t *m, struct worker *w, url_t *url,
             attr_list_t *attributes, filetype_t *ft)
{
    printf("[-] %s\n", url->str);
}

static void
mb_status_cb(metha_t *m, struct worker *w, url_t *url)
{
    printf("[I] URL: %s\n", url->str);
}

static void
mb_error_cb(metha_t *m, const char *s, ...)
{
    char *s2 = 0;
    int sz;

    va_list va;
    va_start(va, s);

    sz = strlen(s)+5;
    if (!(s2 = malloc(sz+1)))
        return;

    memcpy(s2, "[E] ", 4);
    strcpy(s2+4, s);
    sz = strlen(s2);
    *(s2+sz) = '\n';
    *(s2+sz+1) = '\0';

    vfprintf(stderr, s2, va);
    free(s2);

    va_end(va);
}

static void
mb_warning_cb(metha_t *m, const char *s, ...)
{
    char *s2 = 0;
    int sz;

    va_list va;
    va_start(va, s);

    sz = strlen(s)+5;
    if (!(s2 = malloc(sz+1)))
        return;

    memcpy(s2, "[W] ", 4);
    strcpy(s2+4, s);
    sz = strlen(s2);
    *(s2+sz) = '\n';
    *(s2+sz+1) = '\0';

    vfprintf(stderr, s2, va);
    free(s2);

    va_end(va);
}

void
mb_uninit(void)
{
    if (home_conf)
        free(home_conf);
    if (home_scripts)
        free(home_scripts);
    if (install_scripts)
        free(install_scripts);
    if (install_conf)
        free(install_conf);
}

