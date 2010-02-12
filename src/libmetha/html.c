/*-
 * html.c
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "url.h"
#include "urlengine.h"
#include "io.h"
#include "config.h"
#include "worker.h"
#include "builtin.h"

#ifndef HAVE_MEMMEM
#define memmem(x, xz, y, yz) strstr(x, y)
#endif

typedef struct curie_prefix {
    char  *prefix;
    size_t prefix_len;
    char  *url;
    size_t url_len;
} curie_prefix_t;

/* html parser info */
typedef struct info {
    curie_prefix_t *curies;
    int         num_curies;
} info_t;

static void parse_textarea(uehandle_t *ue_h, char *p, size_t sz);
static void parse_script(uehandle_t *ue_h, char *p, size_t sz);
static inline int parse_tag(uehandle_t *ue_h, info_t *info, char *p, size_t sz);
static void* memcpy_tolower(void *dest, const void *source, size_t sz);

/* Names one xml/html element, see lm_html_to_xml() */
struct xml_el {
    const char *p; /* pointer to the name */
    uint32_t    sz; /* length of the name */
};

struct tag {
    void (*handler_cb)(uehandle_t *, char *, size_t); /* function called for the content of this html tag */
    char *name; /* case insensitive name, ie "script" or "textarea" */
    int   name_len; /* for optimization */
};

#define NUM_TAGS (sizeof(tags)/sizeof(struct tag))

/* current list of (partially) parsable tags */
static struct tag tags[] = {
    {&parse_script,        "script",   6},
    {(void (*)(uehandle_t *, char*, size_t))&lm_extract_css_urls, "style",    5},
    {&parse_textarea,      "textarea", 8},
};

/**
 * Copy a memory region, converting uppercase characters 
 * to lowercase. Used by lm_html_to_xml() for HTML tags 
 * and attributes.
 * */
static void*
memcpy_tolower(void *dest, const void *source, size_t sz)
{
    const char *s = (const char *)source;
    char *d       = (char *)dest;

    if (sz) {
        do {
            sz--;
            d[sz] = tolower(s[sz]);
        } while (sz);
    }

    return dest;
}

/** 
 * Default HTML parser.
 **/
M_CODE
lm_parser_html(struct worker *w, struct iobuf *buf,
               struct uehandle *ue_h, struct url *url,
               struct attr_list *al)
{
    int ret;
    char *p   = buf->ptr,
         *e   = buf->ptr+buf->sz,
         *tb  = 0,
         *te  = 0,
         q, *s;
    int type;
    info_t info;

    info.curies = 0;
    info.num_curies = 0;

    for (;p<e;p++) {
        tb = e;
        do {
            if (*p == '<') {
                tb = p;
                s = p+1;
                /* we're inside a < ... > */
                for (;s<e;s++) {
                    if (*s == '=') {
                        /* parse through html attributes, ie href="val" */
                        s++;
                        if ((q = (*s == '"'?'"':(*s == '\''?'\'':'\0')))) {
                            if (!(s = memchr(s, q, e-s)))
                                s = e; /* this will break out of all loops */
                        } else {
                            do s++;
                            while (s < e && *s != '>'
                                    && !isspace(*s));
                            if (*s == '>') {
                                te = s;
                                break;
                            }
                        }
                    }
                    /* try to be fault-tolerant */
                    if (*s == '<')
                        tb = s;
                    else if (*s == '>') {
                        te = s;
                        break;
                    }
                }
            }
            p++;
            /* TODO: extract plain text links */
        } while (p<tb);
        if ((type = parse_tag(ue_h, &info, tb, te-tb)) != -1) {
            do {
                if ((p = memchr(p, '<', e-p))) {
                    if (*(p+1) == '/') {
                        if (e-p < 8) {
                            p=e;
                            break;
                        }
                        else if (strncasecmp(p+2, tags[type].name, tags[type].name_len) == 0) {
                            tags[type].handler_cb(ue_h, te+1, p-(te+1));
                            p += 2+tags[type].name_len+1;
                            /* TODO: handle tags such as </script random> */
                            break;
                        } else
                            p++;
                    } else
                        p++;
                } else {
                    p=e; /* break out of outer loop */
                    break;
                }
            } while (1);
        }
    }

    /* set the attribute 'html' if the
     * target filetype has it */
    lm_attribute_set(al, "html", buf->ptr, buf->sz);

    if (info.num_curies)
        free(info.curies);

    return ret;
}

/** 
 * Callback function for parsing textareas in HTML.
 *
 * Called by lm_parser_html()
 **/
static void
parse_textarea(uehandle_t *h, char *p, size_t sz)
{
    /* TODO: handle textarea, there's probably nothing interesting here though */
}

/** 
 * Callback function for parsing javascript and other
 * <script> elements in HTML.
 *
 * Called by lm_parser_html()
 **/
static void
parse_script(uehandle_t *h, char *p, size_t sz)
{
    /* TODO: handle scripts */
}

/**
 * get the next attribute and its value,
 * pp should be a pointer to the current posiion in the buffer
 * and ee should be a pointer to where the buffer ends
 *
 * this function will skip attributes without values
 **/
static inline M_CODE
tag_next_attr(char **pp,   char **ee,
              char **attr, size_t *attr_len,
              char **val,  size_t *val_len)
{
    char *p = *pp;
    char *e = *ee;
    char *s;
    char  c;

    while (p<e) {
        while (isspace(*p) && p<e)
            p++;
        *attr = p;
        while (p<e) {
            if (isspace(*p)) {
                *attr_len = p-(*attr);
                do p++;
                while (isspace(*p) && p<e);
                if (*p != '=')
                    continue;
                break;
            }
            if (*p == '=') {
                *attr_len = p-(*attr);
                break;
            }

            p++;
        }
        do p++;
        while (isspace(*p) && p<e);
        if (p>=e)
            break;
        s = p;
        if ((c = (*p == '\''?'\'':(*p == '"'?'"':0)))) {
            p++;
            do s++;
            while (s<e && *s != c)
                ;
            *val_len = s-p;
            *pp = s+1;
        } else {
            do { s++; } while (s<e && !isspace(*s) && *s != '>');
            *val_len = s-p;
            *pp = s;
        }
        *val = p;
        return M_OK;
    }

    return M_FAILED;
}

/** 
 * Parse the content and return the index of the type
 * of the given <html-tag attr=val>
 **/
static inline int
parse_tag(uehandle_t *h, info_t *i,
          char *p, size_t len)
{
    int x;
    char *e = p+len;
    size_t val_len, attr_len;
    char *attr, *val;
    p++;
    for (x=0; x<NUM_TAGS; x++) {
        if (tags[x].name_len < len)
            if (strncasecmp(p, tags[x].name, tags[x].name_len) == 0)
                return x;
    }
    if (strncasecmp(p, "html", 4) == 0) {
        p+=4;
        while (tag_next_attr(&p, &e, &attr,
                    &attr_len, &val, &val_len) == M_OK) {
            if (attr_len > 6 && strncasecmp(attr, "xmlns:", 6) == 0) {
                /* curie-stuff, no one uses it yet but people think
                 * i'm cool if i implement it early before it is 
                 * being in use on the web */
                if (!(i->curies = realloc(i->curies,
                        (i->num_curies+1)*sizeof(curie_prefix_t))))
                    return -1;
                i->curies[i->num_curies].prefix = attr+6;
                i->curies[i->num_curies].prefix_len = attr_len-6;
                i->curies[i->num_curies].url = val;
                i->curies[i->num_curies].url_len = val_len;
                i->num_curies++;
            }
        }
    } else {
        do p++; while (p<e && !isspace(*p));
        while (tag_next_attr(&p, &e, &attr,
                    &attr_len, &val, &val_len) == M_OK) {
            if ((attr_len == 4 && strncasecmp(attr, "href", 4) == 0)
                    || (attr_len == 3 && strncasecmp(attr, "src", 3) == 0)) {
                if (*val == '[' && i->num_curies) {
                    for (x=0; x<i->num_curies; x++) {
                        if (val_len > i->curies[x].prefix_len+3) {
                            if (strncasecmp(val+1, i->curies[x].prefix,
                                        i->curies[x].prefix_len) == 0
                                  && *(val+i->curies[x].prefix_len+1) == ':') {
                                char *tmp = malloc(i->curies[x].url_len+val_len-(3+i->curies[x].prefix_len));
                                if (!tmp)
                                    break;
                                memcpy(tmp, i->curies[x].url, i->curies[x].url_len);
                                memcpy(tmp+i->curies[x].url_len, val+2+i->curies[x].prefix_len,
                                        val_len-3-i->curies[x].prefix_len);
                                ue_add(h, tmp, i->curies[x].url_len+val_len-(3+i->curies[x].prefix_len));
                                free(tmp);
                                break;
                            }
                        }
                    }
                } else if (*val != '#') /* skip references to anchors */
                    ue_add(h, val, val_len);
                return -1; /* should we really return here? pretty sure
                              no one would put more than one href attribute 
                              in a tag */
            }
        }
    }
    return -1;
}

#define EST_CAP 8
#define NBUF_CHECKSZ(x) \
    do {                 \
        if (np+(x) >= n+n_cap) { \
            int offs = np-n; \
            if (!(n = realloc(n, (n_cap*=2)))) \
                return M_OUT_OF_MEM; \
            np = n+offs; \
        } \
    } while (0);

#define NUM_EL_NO_CONTENT sizeof(no_content)/sizeof(struct xml_el)
/* list of elements that require a '/' ending them in XML, since they have no "body" */
static const struct xml_el no_content[] =
{
    {"br", 2}, {"hr", 2}, {"img", 3}, {"link", 4}, {"meta", 4},
    {"base", 4}, {"basefont", 8}, {"area", 4}, {"input", 5},
    {0}
};

#define NUM_EL_ENC_CONTENT sizeof(enc_content)/sizeof(struct xml_el)
/* list of elements that need their body XML entity encoded, for 
 * example <script>-elements might contain many combinations of 
 * '<' and '>'. For these elements, nothing between <element>
 * and </element> will be considered XML elements, they will all be 
 * encoded. */
static const struct xml_el enc_content[] =
{
    {"textarea", 8}, {"script", 6}, {"style", 5},
    {0}
};

/** 
 * lm_parser_xmlconv() - former lm_html_to_xml()
 *
 * Convert the data in buf to XML. The function is used to
 * convert HTML to XML. We must do this before we can parse the
 * strings using E4X in javascript.
 *
 * This parser will not extract any URLs, its mere purpose is to
 * be chained and give proper XML data to chained javascript 
 * parsers.
 *
 * - Remove possible "<!DOCTYPE ... >"
 * - Replace '&' with "&amp;", this will fix errors caused by HTML 
 *   entities that are not part of the list of predefined XML entities.
 *   With "&amp;" instead of '&', all HTML-only entities can success-
 *   fully be feed into a spidermonkey XML object. It will also prevent
 *   spidermonkey from reporting errors if an ampersand is used as part
 *   of plain-text. Note that while the result will be "&amp;auml;" for
 *   "&auml;", in the javascript code the XML object will still return
 *   "&auml;" to the script. This is merely for when adding data to the 
 *   XML object.
 * - Add missing slashes to end elements, <br> for example is not 
 *   accepted by XML, so it must be converted to <br />.
 * - Tags inside <script>, <style> and <textarea> must be converted.
 **/
M_CODE
lm_parser_xmlconv(struct worker *w, struct iobuf *buf,
                  struct uehandle *ue_h, struct url *url,
                  struct attr_list *al)
{
    char *p = buf->ptr,
         *e = buf->ptr+buf->sz,
         *n, *s, *as, *ae, *vs, *ve,
         q;
    char *np;
    struct xml_el *est;
    int len;
    int n_cap = buf->sz+128;

    if (!(n = malloc(n_cap)))
        return M_OUT_OF_MEM;
    if (!(est = malloc(sizeof(struct xml_el)*EST_CAP)))
        return M_OUT_OF_MEM;
    
    int est_sz = 0;
    int est_cap = EST_CAP;
    int x;

    for (np=n;p<e;) {
        if (*p != '<') {
            if (est_sz) {
                for (s=p; ; s++) {
                    if (*s == '&') {
                        if (s-p) {
                            NBUF_CHECKSZ(s-p);
                            memcpy(np, p, s-p);
                            np+=s-p;
                        }
                        p = s+1;

                        NBUF_CHECKSZ(5);
                        memcpy(np, "&amp;", 5);
                        np+=5;
                        continue;
                    }
                    if (s>=e || *s == '<') {
                        NBUF_CHECKSZ(s-p);
                        memcpy(np, p, s-p);
                        np+=s-p;
                        break;
                    }
                }
                p = s;
            } else
                p++;
        } else {
            /* XXX: This is a temporary solution for discarding the DOCTYPE */
            if (*(p+1) == '!' && strncasecmp(p+2, "DOCTYPE", 7) == 0) {
                if (!(p = memchr(p, '>', p-e)))
                    return M_ERROR;
                p++;
                continue;
            }
            /* we're inside a < ... >, first we save the element in the element stack */
            p++;
            if (*p != '/') {
restart:
                if (strncmp(p, "!--", 3) == 0) {
                    p = memmem(p+3, e-p, "-->", 3);
                    if (!p)
                        p = e;
                    else p+=3;
                    continue;
                }
                if (*p == '?') {
                    if (!(p = strchr(p+1, '>')))
                        p = e;
                    else p++;
                    continue;
                }

                /* find the length of the element */
                for (s=p; s<e && *s != '/' && *s != '>' && !isspace(*s); s++)
                    ;
                len = s-p;

                /* Workaround for websites that omit <html> and go directly with <body> and <head> etc, 
                 * such as some pages at GOOGLE, ARGH >=O */
                if (!est_sz && (len != 4 || strncasecmp(p, "html", 4) != 0)) {
                    est[0].p = "html";
                    est[0].sz = 4;
                    est_sz = 1;
                    /* we assume the buffer can hold this tiny string */
                    memcpy(np, "<html>", 6);
                    np+=6;
                }

                /* copy the element to the new buffer */
                NBUF_CHECKSZ(len+1);
                memcpy_tolower(np, p-1, len+1);
                np+=len+1;

                int noco = 0;
                int enc=0;
                /* see if this is a no-content element, and if so, then we should add a '/' before
                 * the '>', otherwise just add the element to the element stack */
                for (x=0; x<NUM_EL_NO_CONTENT; x++) {
                    if (len == no_content[x].sz && strncasecmp(p, no_content[x].p, len) == 0) {
                        noco = 1;
                        break;
                    }
                }
                if (!noco) {
                    for (x=0; x<NUM_EL_ENC_CONTENT; x++) {
                        if (len == enc_content[x].sz && strncasecmp(p, enc_content[x].p, len) == 0) {
                            enc = 1;
                            break;
                        }
                    }
                    if (!enc) {
                        if (est_sz >= est_cap && !(est = realloc(est, (est_cap*=2)*sizeof(struct xml_el))))
                            return M_OUT_OF_MEM;

                        est[est_sz].p = p;
                        est[est_sz].sz = len;
                        est_sz ++;
                    }
                }
                for (;s<e;) {
                    while (isspace(*s))
                        s++;
                    as=s;
                    while (isalnum(*s) || *s == ':')
                        s++;
                    ae=s;

                    /* skip possible whitespaces, some people think 
                     * spaces between html attributes, values and the 
                     * equal sign is a good practice... ugh. */
                    while (isspace(*s))
                        s++;
                    if (*s == '=') {
                        /* parse through html attributes, ie href="val" */
                        int n_amp = 0;
                        do s++;
                        while (isspace(*s)); /* again, skip whitespaces */
                        if ((q = (*s == '"'?'"':(*s == '\''?'\'':'\0')))) {
                            s++;
                            vs = s;
                            while (s < e && *s != q) {
                                if (*s == '&')
                                    n_amp ++;
                                s++;
                            }
                            ve = s+1;
                        } else {
                            q = '"';
                            vs = s;
                            do {
                                s++;
                                if (*s == '&')
                                    n_amp ++;
                            } while (s < e && *s != '>'
                                    && !isspace(*s));
                            ve = s;
                        }

                        /* everything fails if xmlns is specified in <html>... */
                        if (strncasecmp(as, "xmlns", 5) != 0) {
                            if (n_amp) {
                                /* copy while converting & to &amp; */
                                NBUF_CHECKSZ((ae-as)+(s-vs)+4+(n_amp*5));
                                *(np++) = ' ';
                                memcpy_tolower(np, as, ae-as);
                                np+=ae-as;
                                *(np++) = '=';
                                *(np++) = q;
                                for (as=vs; as<s; as++) {
                                    if (*as == '&') {
                                        memcpy(np, "&amp;", 5);
                                        np+=5;
                                    } else
                                        *(np++) = *as;
                                }
                                *(np++) = q;
                            } else {
                                NBUF_CHECKSZ((ae-as)+(s-vs)+4);
                                *(np++) = ' ';
                                memcpy_tolower(np, as, ae-as);
                                np+=ae-as;
                                *(np++) = '=';
                                *(np++) = q;
                                memcpy(np, vs, s-vs);
                                np+=s-vs;
                                *(np++) = q;
                            }
                        }
                        s = ve;
                        continue;
                    } else if (*s == '<') {
                        /* try to be fault-tolerant */
                        /* TODO: convert the '<' */
                        est_sz --;
                        p = s+1;
                        goto restart;
                    } else if (*s == '>')
                        break;

                    /* we probably got an html attribute without value, for example:
                     * <a href> or <p blah>
                     * I'm not sure what to do here, probably just discard the attribute... */
                    s++;
                    continue;
                }

                p=s;

                if (noco) {
                    NBUF_CHECKSZ(2);
                    *(np++) = '/';
                } else if (enc) {
                    NBUF_CHECKSZ(1);
                    *(np++) = '>';
                    p++;

                    /* find the ending tag, and convert all tags until found */
                    for (s=p; s<e; s++) {
                        if (*s == '&') {
                            if (s-p) {
                                NBUF_CHECKSZ(s-p);
                                memcpy(np, p, s-p);
                                np+=s-p;
                            }
                            p = s+1;

                            NBUF_CHECKSZ(5);
                            memcpy(np, "&amp;", 5);
                            np+=5;
                        } else if (*s == '<') {
                            if (*(s+1) == '/') {
                                if (strncasecmp(s+2, enc_content[x].p, enc_content[x].sz) == 0) {
                                    s+=2+enc_content[x].sz;
                                    NBUF_CHECKSZ(s-p);
                                    memcpy_tolower(np, p, s-p);
                                    np+=s-p;
                                    while (s < e) {
                                        if (*s == '>')
                                            break;

                                        s++;
                                    }
                                    break;
                                }
                            }
                            NBUF_CHECKSZ(s-(p+4));
                            memcpy(np, p, s-p);
                            np+=s-p;
                            memcpy(np, "&lt;", 4);
                            np+=4;
                            p=s+1;
                        }
                    }
                    p = s+1;
                    NBUF_CHECKSZ(1);
                    *(np++) = '>';
                    continue;
                } else
                    NBUF_CHECKSZ(1);
                *(np++) = '>';
            } else {
                /**
                 * An element gets closed here using </element>, we need to 
                 * verify that this closing element matches very last element that
                 * was opened, or the code will not be considered by SpiderMonkey 
                 * as proper XML and thus everything would fail. If this closing 
                 * element does not match the last opened element, we fall back to 
                 * explicitly closing all elements in the stack (descending) until
                 * we reach the element in question. While this is a pretty ugly 
                 * "workaround", it can't be much uglier than the HTML code itself, 
                 * since Mr. Markup obviously failed to close elements in order.
                 **/
                p++;
                /* start by getting the length of this element */
                for (s=p; s<e && *s != '>' && !isspace(*s); s++)
                    ;
                len = s-p;

                x=est_sz-1;
                /* find a matching tag in the stack */
                while ((est[x].sz != len || strncasecmp(p, est[x].p, est[x].sz) != 0)) {
                    x--;
                    if (x <= 0) {
                        /* ok, so Mr. Markup tried to close an element he never opened...
                         * we simply discard this closing-tag */
                        x = -1;
                        break;
                    }
                }
                if (x != -1) {
                    do {
                        est_sz --;
                        /* check the target buffer length */
                        NBUF_CHECKSZ(3+est[est_sz].sz);
                        /* insert a closing tag here */
                        *(np++) = '<';
                        *(np++) = '/';
                        memcpy_tolower(np, est[est_sz].p, est[est_sz].sz);
                        np+=est[est_sz].sz;
                        *(np++) = '>';
                    } while (est_sz != x);
                }
            }


            while (p < e) {
                if (*p == '>')
                    break;

                p++;
            }

            /* p will now point to after the '>' */
            p++;
        }
    }

    /* end all non-terminated elements, in order */
    while (est_sz) {
        est_sz --;
        NBUF_CHECKSZ(3+est[est_sz].sz);
        *(np++) = '<';
        *(np++) = '/';
        memcpy(np, est[est_sz].p, est[est_sz].sz);
        np+=est[est_sz].sz;
        *(np++) = '>';
    }

    *(np++) = '\0';

    free(est);
    free(buf->ptr);

    buf->ptr = n;
    buf->cap = n_cap;
    buf->sz = np-n-1;
    return M_OK;
}

