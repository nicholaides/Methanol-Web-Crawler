/*-
 * entityconv.c
 * This file is part of libmetha
 *
 * Copyright (c) 2009, Emil Romanus <emil.romanus@gmail.com>
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

#include <string.h>
#include <iconv.h>
#include <ctype.h>

#include "errors.h"
#include "worker.h"
#include "urlengine.h"
#include "io.h"

static int entity_hash(const char *s, int size);
static int unicode_to_utf8(uint16_t v, char *out);

typedef
struct html_entity {
    const char *ident;
    uint16_t    unicode;
} entity_t;

struct hashtbl_pos {
    int        count;
    entity_t **ptr;
};

static struct hashtbl_pos e_tbl[128];
static int                hashtbl_initialized = 0;

#define NUM_ENTITIES (sizeof(entities)/sizeof(entity_t))

/** 
 * lmetha_global_init() will generate a 
 * hash table from these values
 **/
static const entity_t entities[] =
{
    /*{"gt", 0x003E},*/
    /*{"lt", 0x003C},*/
    {"Aacute", 0x00C1},
    {"aacute", 0x00E1},
    {"Acirc", 0x00C2},
    {"acirc", 0x00E2},
    {"acute", 0x00B4},
    {"AElig", 0x00C6},
    {"aelig", 0x00E6},
    {"Agrave", 0x00C0},
    {"agrave", 0x00E0},
    {"alefsym", 0x2135},
    {"Alpha", 0x0391},
    {"alpha", 0x03B1},
    {"amp", 0x0026},
    {"and", 0x2227},
    {"ang", 0x2220},
    {"apos", 0x0027},
    {"Aring", 0x00C5},
    {"aring", 0x00E5},
    {"asymp", 0x2248},
    {"Atilde", 0x00C3},
    {"atilde", 0x00E3},
    {"Auml", 0x00C4},
    {"auml", 0x00E4},
    {"bdquo", 0x201E},
    {"Beta", 0x0392},
    {"beta", 0x03B2},
    {"brvbar", 0x00A6},
    {"bull", 0x2022},
    {"cap", 0x2229},
    {"Ccedil", 0x00C7},
    {"ccedil", 0x00E7},
    {"cedil", 0x00B8},
    {"cent", 0x00A2},
    {"Chi", 0x03A7},
    {"chi", 0x03C7},
    {"circ", 0x02C6},
    {"clubs", 0x2663},
    {"cong", 0x2245},
    {"copy", 0x00A9},
    {"crarr", 0x21B5},
    {"cup", 0x222A},
    {"curren", 0x00A4},
    {"dagger", 0x2020},
    {"Dagger", 0x2021},
    {"darr", 0x2193},
    {"dArr", 0x21D3},
    {"deg", 0x00B0},
    {"Delta", 0x0394},
    {"delta", 0x03B4},
    {"diams", 0x2666},
    {"divide", 0x00F7},
    {"Eacute", 0x00C9},
    {"eacute", 0x00E9},
    {"Ecirc", 0x00CA},
    {"ecirc", 0x00EA},
    {"Egrave", 0x00C8},
    {"egrave", 0x00E8},
    {"empty", 0x2205},
    {"emsp", 0x2003},
    {"ensp", 0x2002},
    {"Epsilon", 0x0395},
    {"epsilon", 0x03B5},
    {"equiv", 0x2261},
    {"Eta", 0x0397},
    {"eta", 0x03B7},
    {"ETH", 0x00D0},
    {"eth", 0x00F0},
    {"Euml", 0x00CB},
    {"euml", 0x00EB},
    {"euro", 0x20AC},
    {"exist", 0x2203},
    {"fnof", 0x0192},
    {"forall", 0x2200},
    {"frac12", 0x00BD},
    {"frac14", 0x00BC},
    {"frac34", 0x00BE},
    {"frasl", 0x2044},
    {"Gamma", 0x0393},
    {"gamma", 0x03B3},
    {"ge", 0x2265},
    {"harr", 0x2194},
    {"hArr", 0x21D4},
    {"hearts", 0x2665},
    {"hellip", 0x2026},
    {"Iacute", 0x00CD},
    {"iacute", 0x00ED},
    {"Icirc", 0x00CE},
    {"icirc", 0x00EE},
    {"iexcl", 0x00A1},
    {"Igrave", 0x00CC},
    {"igrave", 0x00EC},
    {"image", 0x2111},
    {"infin", 0x221E},
    {"int", 0x222B},
    {"Iota", 0x0399},
    {"iota", 0x03B9},
    {"iquest", 0x00BF},
    {"isin", 0x2208},
    {"Iuml", 0x00CF},
    {"iuml", 0x00EF},
    {"Kappa", 0x039A},
    {"kappa", 0x03BA},
    {"Lambda", 0x039B},
    {"lambda", 0x03BB},
    {"lang", 0x2329},
    {"laquo", 0x00AB},
    {"larr", 0x2190},
    {"lArr", 0x21D0},
    {"lceil", 0x2308},
    {"ldquo", 0x201C},
    {"le", 0x2264},
    {"lfloor", 0x230A},
    {"lowast", 0x2217},
    {"loz", 0x25CA},
    {"lrm", 0x200E},
    {"lsaquo", 0x2039},
    {"lsquo", 0x2018},
    {"macr", 0x00AF},
    {"mdash", 0x2014},
    {"micro", 0x00B5},
    {"middot", 0x00B7},
    {"minus", 0x2212},
    {"Mu", 0x039C},
    {"mu", 0x03BC},
    {"nabla", 0x2207},
    {"nbsp", 0x00A0},
    {"ndash", 0x2013},
    {"ne", 0x2260},
    {"ni", 0x220B},
    {"not", 0x00AC},
    {"notin", 0x2209},
    {"nsub", 0x2284},
    {"Ntilde", 0x00D1},
    {"ntilde", 0x00F1},
    {"Nu", 0x039D},
    {"nu", 0x03BD},
    {"Oacute", 0x00D3},
    {"oacute", 0x00F3},
    {"Ocirc", 0x00D4},
    {"ocirc", 0x00F4},
    {"OElig", 0x0152},
    {"oelig", 0x0153},
    {"Ograve", 0x00D2},
    {"ograve", 0x00F2},
    {"oline", 0x203E},
    {"Omega", 0x03A9},
    {"omega", 0x03C9},
    {"Omicron", 0x039F},
    {"omicron", 0x03BF},
    {"oplus", 0x2295},
    {"or", 0x2228},
    {"ordf", 0x00AA},
    {"ordm", 0x00BA},
    {"Oslash", 0x00D8},
    {"oslash", 0x00F8},
    {"Otilde", 0x00D5},
    {"otilde", 0x00F5},
    {"otimes", 0x2297},
    {"Ouml", 0x00D6},
    {"ouml", 0x00F6},
    {"para", 0x00B6},
    {"part", 0x2202},
    {"permil", 0x2030},
    {"perp", 0x22A5},
    {"Phi", 0x03A6},
    {"phi", 0x03C6},
    {"Pi", 0x03A0},
    {"pi", 0x03C0},
    {"piv", 0x03D6},
    {"plusmn", 0x00B1},
    {"pound", 0x00A3},
    {"prime", 0x2032},
    {"Prime", 0x2033},
    {"prod", 0x220F},
    {"prop", 0x221D},
    {"Psi", 0x03A8},
    {"psi", 0x03C8},
    {"quot", 0x0022},
    {"radic", 0x221A},
    {"rang", 0x232A},
    {"raquo", 0x00BB},
    {"rarr", 0x2192},
    {"rArr", 0x21D2},
    {"rceil", 0x2309},
    {"rdquo", 0x201D},
    {"real", 0x211C},
    {"reg", 0x00AE},
    {"rfloor", 0x230B},
    {"Rho", 0x03A1},
    {"rho", 0x03C1},
    {"rlm", 0x200F},
    {"rsaquo", 0x203A},
    {"rsquo", 0x2019},
    {"sbquo", 0x201A},
    {"Scaron", 0x0160},
    {"scaron", 0x0161},
    {"sdot", 0x22C5},
    {"sect", 0x00A7},
    {"shy", 0x00AD},
    {"Sigma", 0x03A3},
    {"sigma", 0x03C3},
    {"sigmaf", 0x03C2},
    {"sim", 0x223C},
    {"spades", 0x2660},
    {"sub", 0x2282},
    {"sube", 0x2286},
    {"sum", 0x2211},
    {"sup", 0x2283},
    {"sup1", 0x00B9},
    {"sup2", 0x00B2},
    {"sup3", 0x00B3},
    {"supe", 0x2287},
    {"szlig", 0x00DF},
    {"Tau", 0x03A4},
    {"tau", 0x03C4},
    {"there4", 0x2234},
    {"Theta", 0x0398},
    {"theta", 0x03B8},
    {"thetasym", 0x03D1},
    {"thinsp", 0x2009},
    {"THORN", 0x00DE},
    {"thorn", 0x00FE},
    {"tilde", 0x02DC},
    {"times", 0x00D7},
    {"trade", 0x2122},
    {"Uacute", 0x00DA},
    {"uacute", 0x00FA},
    {"uarr", 0x2191},
    {"uArr", 0x21D1},
    {"Ucirc", 0x00DB},
    {"ucirc", 0x00FB},
    {"Ugrave", 0x00D9},
    {"ugrave", 0x00F9},
    {"uml", 0x00A8},
    {"upsih", 0x03D2},
    {"Upsilon", 0x03A5},
    {"upsilon", 0x03C5},
    {"Uuml", 0x00DC},
    {"uuml", 0x00FC},
    {"weierp", 0x2118},
    {"Xi", 0x039E},
    {"xi", 0x03BE},
    {"Yacute", 0x00DD},
    {"yacute", 0x00FD},
    {"yen", 0x00A5},
    {"yuml", 0x00FF},
    {"Yuml", 0x0178},
    {"Zeta", 0x0396},
    {"zeta", 0x03B6},
    {"zwj", 0x200D},
    {"zwnj", 0x200C},
};

/* create a hash from a given string, very simple
 * hashing algorithm to avoid complexity */
static int
entity_hash(const char *s, int size)
{
    int   h = 0xc8;
    char *e;
    char *p = (char*)s;

    for (e=p+size; p<e; p++)
        h += (h<<1) ^ *p;
    h &= 0x7f;
    return h;
}

/** 
 * called by lmetha_global_init() to create initialize
 * the hash table
 **/
M_CODE
lm_entity_hashtbl_init(void)
{
    int x;
    int hash;

    if (hashtbl_initialized == 1)
        return M_OK;
#ifdef DEBUG
    fprintf(stderr, "* global: creating entity hash table\n");
#endif

    for (x=0; x<NUM_ENTITIES; x++) {
        hash = entity_hash(entities[x].ident, strlen(entities[x].ident));
        e_tbl[hash].count ++;
        if (!(e_tbl[hash].ptr = realloc(e_tbl[hash].ptr,
                            e_tbl[hash].count*sizeof(entity_t*))))
            return M_OUT_OF_MEM;
        e_tbl[hash].ptr[e_tbl[hash].count-1] = (entity_t*)&entities[x];
    }

    hashtbl_initialized = 1;
    return M_OK;
}

/** 
 * called by lmetha_global_cleanup() to free
 * the hash table
 **/
void
lm_entity_hashtbl_cleanup(void)
{
    int x;
    if (hashtbl_initialized == 1) {
        for (x=0; x<128; x++) {
            if (e_tbl[x].count)
                free(e_tbl[x].ptr);
        }
        hashtbl_initialized = 0;
    }
}
/** 
 * convert the given 16-bit unicode value to an UTF-8 
 * char, write the result to the buffer pointed to by
 * 'out' and return how many bytes that were 
 * written.
 **/
static int
unicode_to_utf8(uint16_t v, char *out)
{
    if (v < 0x0080) {
        *out = v;
        return 1;
    } else if (v < 0x0800) {
        *out     = 0xc0 | (v >> 6);
        *(out+1) = 0x80 | (v & 0x3f);
        return 2;
    }

    *out = 0xe0 | (v >> 12);
    *(out+1) = 0x80 | ((v >> 6) & 0x3f);
    *(out+2) = 0x80 | (v & 0x3f);

    return 3;
}

/** 
 * convert sgml entities to their corresponding UTF-8 
 * character, utf8conv
 **/
M_CODE
lm_parser_entityconv(worker_t *w, iobuf_t *buf,
                     uehandle_t *ue_h, url_t *url,
                     attr_list_t *al)
{
    char *p;
    char *s;
    char *n;
    char *e;
    char *last;
    int h;
    int count;
    int x;
    entity_t *ent;
    e = (last = p = n = buf->ptr) + buf->sz;

    while ((n = memchr(n, '&', e-n))) {
        n++;
        s = n;
        if (*s == '#') {
            /* convert numeric entities */
            n++;
        } else {
            while (isalnum(*n))
                n++;
            if (*n != ';')
                continue;
            *n = '\0';
            h = entity_hash(s, n-s);
            for (x = 0, count = e_tbl[h].count;
                    x<count; x++) {
                ent = e_tbl[h].ptr[x];
                if (!ent->ident) {
                    *n = ';';
                    break;
                }
                if (strcmp(ent->ident, s) == 0) {
                    memcpy(p, last, s-1-last);
                    p += s-1-last;
                    p += unicode_to_utf8(ent->unicode, p);
                    last = n+1;
                    break;
                }
            }
        }
    }
    memcpy(p, last, e-last);
    buf->sz = (p+(e-last))-buf->ptr;

    return M_OK;
}

