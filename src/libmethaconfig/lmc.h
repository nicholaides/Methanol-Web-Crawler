/*-
 * lmc.h
 * This file is part of libmethaconfig
 *
 * Copyright (C) 2009, Emil Romanus <emil.romanus@gmail.com>
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
 * 
 */

#ifndef _LMC__H_
#define _LMC__H_

#include <stdint.h>
#include <stdlib.h>
#include "../libmetha/errors.h"

#define LMC_MAX_ERR_LEN 256

/* macros for declaring option specifications */

#define LMC_OPT_END {.name=(char*)0}
#define LMC_OPT_ARRAY(name, callback) \
    { \
        (name), LMC_VAL_T_ARRAY, \
        .value.array_set = (callback) \
    }

#define LMC_OPT_STRING(name, offset) \
    { \
        (name), LMC_VAL_T_STRING, \
        .value.offs = (offset) \
    }

#define LMC_OPT_UINT(name, offset) \
    { \
        (name), LMC_VAL_T_UINT, \
        .value.offs = (offset) \
    }

#define LMC_OPT_FLAG(name, f) \
    { \
        (name), LMC_VAL_T_FLAG, \
        .value.flag = (f) \
    }

#define LMC_OPT_EXTRA(name, fn) \
    { \
        (name), LMC_VAL_T_EXTRA, \
        .value.set = (fn) \
    }

#define LMC_DIRECTIVE(nm, fn) \
    { \
        .name = (nm), \
        .callback = (M_CODE (*)(void *, const char *))(fn) \
    }

/* option value types, macros above
 * should be used */
enum {
    LMC_VAL_T_UINT,
    LMC_VAL_T_ARRAY,
    LMC_VAL_T_STRING,
    LMC_VAL_T_EXTRA,
    LMC_VAL_T_FLAG,
};

/** 
 * lmc_opt describes a class option,
 * how it should be set and what values it
 * expects
 *
 * set:
 *   set function used when setting values
 *   of type LMC_VAL_T_EXTRA, such as UMEX
 * array_set:
 *   function used for setting an array
 * offs:
 *   offset to the element associated with
 *   this option in the class. use offsetof()
 * flag:
 *   a 32-bit integer that should contain
 *   a bit mask which to enable or disable
 *   depending on whther this option is 
 *   set to true or false
 **/
struct lmc_opt {
    const char       *name;
    int               type;
    union {
        M_CODE     (*set)(void*, const char *);
        M_CODE     (*array_set)(void*, char **, int);
        uintptr_t    offs;
        uint32_t     flag;
    } value;
};

/** 
 * scopes are a lot like classes except that 
 * it doesn't take a name, and each time variables 
 * are changed in a scope, the same "object" is
 * modified. 
 **/
struct lmc_scope {
    const char         *name;
    struct lmc_opt     opts[];
};

/** 
 * lmc_class
 *
 * class callback functions for creating and 
 * modifying an object type such as 'filetype'
 * or 'crawler' in libmetha
 **/
struct lmc_class {
    const char           *name;
    /*
     * Callback functions for creating, adding, duplicating and 
     * setting this type of object.
     *
     * find_cb - used for finding an existing object with the
     * given name (for the extend and override keywords) 
     *
     * add_cb - add a configured object to the lmc parsers attached
     * root object. First argument is the root object, second arg is
     * the object itself. The root object MUST free this added object
     * later, the lmc_parser will not free it once it's given to
     * the root object.
     *
     * zero_cb - called when an object should be zeroed, used 
     * for the override keyword 
     *
     * copy_cp - copy all settings from arg2 to arg1, should return M_OK 
     *
     * constructor_cb - create a new object of this type, arg1 is name,
     * arg2 is the length of the name 
     *
     * destructor_cb - free the given object, this function will never be
     * called if add_cb has been used previously on the object 
     */
    void               *(*find_cb)(void *, const char *);
    void               *(*add_cb)(void *, void*);
    void                (*zero_cb)(void *);
    int                 (*copy_cb)(void *, void*);
    void               *(*constructor_cb)(const char *, uint32_t);
    void                (*destructor_cb)(void*);
     /* offset to the 'flags' element of the object. The flag element must
      * be a 32-bit integer */
    uintptr_t             flags_offs;

    /* list of options */
    struct lmc_opt opts[];
};

/** 
 * directives are like the #include or #define in C, 
 * but for configuration files here.
 *
 * callback will be sent with first argument set to
 * the attached root object that is being configured,
 * second argument will be the string given to the
 * directive
 **/
struct lmc_directive {
    const char         *name;
    M_CODE            (*callback)(void *, const char *);
};

typedef struct lmc_parser {
    struct lmc_class     **classes;
    unsigned int           num_classes;
    struct lmc_scope     **scopes;
    unsigned int           num_scopes;
    struct lmc_directive **directives;
    unsigned int           num_directives;

    /* pointer to the object to configure */
    void     *root;
    char     *last_error;
} lmc_parser_t;

lmc_parser_t *lmc_create(void *root_object);
void   lmc_destroy(lmc_parser_t *lmc);
M_CODE lmc_parse(lmc_parser_t *lmc, const char *name, const char *buf, size_t bufsz);
M_CODE lmc_parse_file(lmc_parser_t *lmc, const char *filename);
M_CODE lmc_add_scope(lmc_parser_t *lmc, const struct lmc_scope *scope);
M_CODE lmc_add_class(lmc_parser_t *lmc, const struct lmc_class *cl);
M_CODE lmc_add_directive(lmc_parser_t *lmc, const struct lmc_directive *d);

#endif
