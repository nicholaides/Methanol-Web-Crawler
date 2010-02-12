/*-
 * help.c
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

#include <jsapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curlver.h>
#include <dirent.h>

#include "config.h"

extern char *home_conf;
extern int   home_conf_sz;
extern char *install_conf;
extern int   install_conf_sz;
extern char *home_scripts;
extern int   home_scripts_sz;
extern char *install_scripts;
extern int   install_scripts_sz;

#define NUM_OPTS (sizeof(opts)/sizeof(struct opt_help))
static struct opt_help {
    int         arg;
    const char  short_name;
    const char *long_name;
    const char *description;
} opts[] = {
    {
        1, 0, "dynamic-url",
        "Specify how Methabot should handle urls that are considered dynamic.\n"
        "If a URL contains a  question  mark  (?)  after its file name, it is\n"
        "considered dynamic.  Note  that  this  option  applies only when the\n"
        "protocol is HTTP.\n\n"
        "Value can be any of:\n"
        "  lookup       Methabot will perform a HTTP HEAD to determine type.\n"
        "  discard      Discard the URL.\n"
        "  @...         Replace the '...' with the name of a defined filetype\n"
        "               for Methabot to assume the URL is of this type.\n"
    }, {
        1, 'n', "num-workers",
        "Set this option to how many workers you want Methabot to launch."
        "\n\nValue must be 1 or greater.\n"
    }, {
        1, 'a', "user-agent",
        "This option  affects  the  \"User-Agent\"  header  in  HTTP.  Some web\n"
        "servers deny access  from  suspected  web  crawlers, use this option\n"
        "to disguise Methabot.\n\n"
        "Default for libmetha is \"libmetha-agent/" PACKAGE_VERSION "\".\n"
        "Default for Methabot is \"Methabot/" PACKAGE_VERSION "\".\n"
    }, {
        1, 'N', "num-pipelines",
        "Set this option to an  integer  value to specify how many concurrent\n"
        "HTTP HEAD requests  Methabot  is  allowed  to  do. HEAD requests are\n"
        "sometimes necessary for Methabot  to be able to determine the actual\n"
        "type of a file.\n\n"
        "It is recommended not to set this value too high, as web admins will\n"
        "most likely ban your IP if you crawl their websites too fast.\n\n"
        "Default is 8.\n"
    }, {
        0, 'e', "external",
        "Enable this option  if  you  would  like  to  disable  Methabot from\n"
        "discarding URLs that have  a  foreign  host name (external URLs). By\n"
        "default, Methabot will only  crawl  web pages internally and discard\n"
        "external URLs.\n\n"
        "Also see help section for 'external-peek'.\n"
    }, {
        1, 'p', "external-peek",
        "This option allows Methabot to sneak-peek on external URLs. A URL is\n"
        "considered external if its  host  name differs from the current URL.\n"
        "If this option is set,  Methabot  will  take a quick look at the URL\n"
        "and then return back to the previous URL.\n\n"
        "Set this option  to  an  integer  value  specifying  the depth limit\n"
        "before the worker should return from the external host.\n\n"
        "If this option is used  in  combination  with the 'external' option,\n"
        "then Methabot will queue those URLs found below the specified depth,\n"
        "and continue with them once it is out of URLs to the current host.\n"
        "\n"
        "Also see help section for 'external'.\n"
    }, {
        1, 'D', "depth-limit",
        "By default, this option is set to  1. If you want infinite crawling,\n"
        "set this option to 0.\n\n"
    }, {
        0, 'c', "enable-cookies",
        "If this option is set, Methabot will  automatically receive and send\n"
        "cookie information for each  website.  Cookies  are set when an HTTP\n"
        "server send the header Set-Cookie.\n"
    }
};

void
mb_version(void)
{
    printf("methabot libmetha-%s/%s [%s, pthreads, epoll, libcurl-%s]\n",
            (sizeof(void*)==8?"x86_64":(sizeof(void*)==32?"x86":"")),
            PACKAGE_VERSION,
            JS_GetImplementationVersion(),
            LIBCURL_VERSION);
}

void
mb_examples(void)
{
    printf("%s",
           "Read local HTML file and build external URLs from the links:\n"
           "$ cat htmlfile.html | mb - --base-url=http://anyurl.com/\n"
           "\n"
           "Download MP3 files:\n"
           "$ mb -d -t mp3 anyurl.com\n"
           "\n"
           "Download any type of audio file:\n"
           "$ mb -d :audio anyurl.com\n"
           "\n"
           "Display google search results for the given search string:\n"
           "$ mb :google \"google search string\"\n"
           "\n"
           "Peek on all external URLs found on the given page, and download all image files:\n"
           "$ mb :image -d -p 1 anyurl.com\n"
           );
}

void
mb_info(void)
{
    DIR *dir;
    struct dirent *dp;
    int  o=1;

    printf("Path information:\n");

    printf("* user-config:   %s\n", home_conf);
    printf("* user-script:   %s\n", home_scripts);
    printf("* system-config: %s\n", install_conf);
    printf("* system-script: %s\n", install_scripts);
    printf("* modules:       %s\n", METHA_MODULE_DIR);

    if ((dir = opendir(install_conf)) != 0) {
        while ((dp = readdir(dir)) != 0) {
            if (o) {
                printf("\nDefault configuration files:\n");
                o = 0;
            }
            char *nm = strdup(dp->d_name);
            char *p;
            if ((p = strrchr(nm, '.')) && strcmp(p, ".conf") == 0) {
                *p = '\0';
                printf("* %s\n", nm);
            }
            free(nm);
        }
        closedir(dir);
    }

    o = 1;
    if ((dir = opendir(home_conf)) != 0) {
        while ((dp = readdir(dir)) != 0) {
            if (o) {
                printf("\nUser configuration files:\n");
                o = 0;
            }
            char *nm = strdup(dp->d_name);
            char *p;
            if ((p = strrchr(nm, '.')) && strcmp(p, ".conf") == 0) {
                *p = '\0';
                printf("* %s\n", nm);
            }
            free(nm);
        }
        closedir(dir);
    }

    return;
}

void
mb_license(void)
{
    printf("%s",
       "Copyright (c) 2008, Emil Romanus <emil.romanus@gmail.com>\n\n"

       "Permission to use, copy, modify, and/or distribute this software for any\n"
       "purpose with or without fee is hereby granted, provided that the above\n"
       "copyright notice and this permission notice appear in all copies.\n\n"

       "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"
       "WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"
       "MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n"
       "ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"
       "WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"
       "ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n"
       "OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
       );
}

void
mb_help(void)
{
    printf("%s",
       "usage: mb [:config] [options...] url\n"
       "Crawler options: \n"
       " -M, --mode             <mode> Crawling mode (aggressive, friendly, coward)\n"
       " -D, --depth-limit       <int> Depth limit (default is 1, 0 is infinite)\n"
       " -e, --external                If set, external URLs will not be discarded\n"
       " -j, --jail                    Restrict the crawling to only subfolders\n"
       "     --spread                  Spread workers on multiple hosts\n"
       " -p, --external-peek     <int> Peek at external URLs for specified depth\n"
       " -r, --robotstxt               Enable robots.txt fetching and parsing\n"
       "     --dynamic-url       <str> How to handle dynamic URLs (containing a '?')\n"
       "     --extless-url       <str> How to handle extensionless file URLs\n"
       "     --dir-url           <str> How to handle directory URLs (ending with '/')\n"
       "     --unknown-url       <str> How to handle completely unknown URLs\n"
       "     --default-handler  <name> Set the default handler for this crawler\n"
       "     --crawler          <name> Set to the name of the crawler to modify\n"
       "Filetype options: \n"
       " -t, --extensions       <list> List of file extensions\n"
       " -x, --expr             <expr> UMEX expression\n"
       " -m, --mimetypes        <list> List of MIME types\n"
       "     --parser           <name> Set the parser\n"
       "     --handler          <name> Set the handler for this filetype\n"
       "     --filetype         <name> Set to the name of the filetype to modify\n"
       "Other options: \n"
/*       " -E, --global-expr      <expr> Global URL expression\n"*/
       " -s, --silent                  Don't display as much output\n"
       " -N, --num-pipelines     <int> Max concurrent HEAD requests (default: 8)\n"
       " -n, --num-workers       <int> Max concurrent worker threads (default: 1)\n"
       " -a, --user-agent        <str> Set user agent\n"
       " -b, --base-url          <url> Build initial links using given URL\n"
       " -d, --download                Download matched URLs without parsers\n"
       " -c, --enable-cookies          Enable automatic cookie handling\n"
       " -T, --type           <string> Filetype of first URL(s)/stdin\n"
       "     --config          <files> Relative or absolute path to a config file\n"
       "     --examples                Example usage\n"
       "     --info                    Output install/build/config/run information\n"
       "     --proxy   <user:pwd@host> Set proxy server\n"
       "     --license                 Show the Methabot license\n"
/*       "     --modules          <list> Specify a list of extra modules to load\n" */
       "     --io-verbose              Display network IO information\n"
       " -v  --version                 Print version information\n"
       " -C  --working-dir             Change the working directory\n"
#ifdef DEBUG
       " -%                      <int> Print debug information\n"
#endif
       " -                             Read from stdin\n\n"

       "For help about a specific option, run `mb --help opt-name', for example:\n"
       "`mb --help dynamic-url'\n"
      );
    return;
}

void
mb_option_help(const char *s)
{
    int x;
    struct opt_help *o = 0;

    if (strlen(s) == 1) {
        for (x=0; x<NUM_OPTS; x++) {
            if (opts[x].short_name == *s) {
                o = &opts[x];
                break;
            }
        }
    } else {
        for (x=0; x<NUM_OPTS; x++) {
            if (strcmp(opts[x].long_name, s) == 0) {
                o = &opts[x];
                break;
            }
        }
    }

    if (!o) {
        printf("No help available for this option. Try looking at:\nhttp://bithack.se/projects/methabot/\n");
    } else {
        if (o->arg) {
            if (o->short_name)
                printf("usage: mb --%s=Value\n       mb -%c Value\n\n", o->long_name, o->short_name);
            else
                printf("usage: mb --%s=Value\n\n", o->long_name);
            printf("%s", o->description);
        } else {
            if (o->short_name)
                printf("usage: mb --%s\n       mb -%c\n\n", o->long_name, o->short_name);
            else
                printf("usage: mb --%s\n\n", o->long_name);
            printf("%s", o->description);
        }
    }
}
