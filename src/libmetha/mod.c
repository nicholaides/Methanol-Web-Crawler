/*-
 * mod.c
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

#include "metha.h"
#include "mod.h"

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

/**
 * Load the given module. 'name' should either contain the 
 * aboslute path to the module, or a name of a module that
 * can be found in the module dir (default /usr/lib/metha/modules/)
 **/
M_CODE
lmetha_load_module(struct metha *m, const char *name)
{
    char *path;
    lm_mod_t *n;

    if (*name == '/')
        path = strdup(name);
    else
        asprintf(&path, "%s/%s.so", m->module_dir, name);

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) loading module '%s'\n", m, path);
#endif

    if (!(n = lm_mod_load(m, path))) {
        LM_ERROR(m, "%s", dlerror());
        free(path);
        return M_FAILED;
    }

    if (!(m->modules = realloc(m->modules, (m->num_modules+1)*sizeof(lm_mod_t))))
        return M_OUT_OF_MEM;

    m->modules[m->num_modules] = n;
    m->num_modules ++;

    return M_OK;
}

lm_mod_t*
lm_mod_load(struct metha *m, const char *file)
{
    lm_mod_t *ret = malloc(sizeof(lm_mod_t));

    if (!ret)
        return 0;

    if (!(ret->handle = dlopen(file, RTLD_LAZY))) {
        free(ret);
        return 0;
    }

    if (!(ret->props = dlsym(ret->handle, "__lm_module_properties"))) {
        dlclose(ret->handle);
        free(ret);
        return 0;
    }

    /**
     * call possible init function
     **/
    if (ret->props->init)
        if (ret->props->init(m) != M_OK)
            return 0;

    return ret;
}

void
lm_mod_unload(struct metha *m, lm_mod_t *mm)
{
    if (mm->props->uninit) {
        mm->props->uninit(m);
    }
    dlclose(mm->handle);
    free(mm);
}

