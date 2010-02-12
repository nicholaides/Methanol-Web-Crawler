#ifndef _METHA__H_
#define _METHA__H_
/**
 * metha.h - Tue Jun 23 15:00:05 CEST 2009
 * Automatically generated for modules and clients.
 * License: http://bithack.se/projects/methabot/license.html
 **/

#include "errors.h"
#include "config.h"
#include "module.h"
#include "lmopt.h"

struct metha;
struct filetype;
struct crawler;
struct io;

typedef struct metha metha_t;
typedef struct crawler crawler_t;
typedef struct filetype filetype_t;
typedef struct io io_t;

typedef struct iobuf {
    char  *ptr;
    size_t sz;
    size_t cap;
} iobuf_t;

M_CODE   lmetha_global_init(void);
void     lmetha_global_cleanup(void);
metha_t *lmetha_create(void);
M_CODE   lmetha_setopt(metha_t *m, LMOPT opt, ...);
void     lmetha_destroy(metha_t *m);
M_CODE   lmetha_exec(metha_t *m, int argc, const char **argv); 
M_CODE   lmetha_exec_provided(metha_t *m, const char *base_url, const char *buf, size_t len);
M_CODE   lmetha_exec_async(metha_t *m, int argc, const char **argv);
M_CODE   lmetha_register_worker_object(metha_t *m, const char *name, JSClass *class);
M_CODE   lmetha_register_jsfunction(metha_t *m, const char *name, JSNative fun, unsigned argc);
M_CODE   lmetha_init_jsclass(metha_t *m, JSClass *class, JSNative constructor, uintN nargs, JSPropertySpec *ps, JSFunctionSpec *fs, JSPropertySpec *static_ps, JSFunctionSpec *static_fs);
M_CODE   lmetha_signal(metha_t *m, int sig);
M_CODE   lmetha_load_config(metha_t *m, const char *file);
M_CODE   lmetha_reset(metha_t *m);
M_CODE   lmetha_read_config(metha_t *m, const char *buf, int size);
M_CODE    lmetha_load_module(struct metha *m, const char *name);
#endif
