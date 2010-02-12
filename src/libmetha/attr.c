/*-
 * attr.c
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

#include "attr.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/** 
 * Set the given attribute 'attribute' to a copy
 * of the given buffer 'value' of size 'size'. If
 * the attribute does not exist in the attribute
 * list, M_FAILED will be returned. Otherwise, the
 * corresponding attr_t in the list is set to the
 * value and M_OK is returned.
 **/
M_CODE
lm_attribute_set(attr_list_t *al, const char *attribute,
                 const char *value, unsigned size)
{
    int     x;
    attr_t *a;
    int     len = strlen(attribute);

    for (x=0; x<al->num_attributes; x++) {
        a = &al->list[x];
        if (strncmp(a->name, attribute, len) == 0
                && (a->name[len] == '\0' || isspace(a->name[len]))) {
            /* found a matching attribute */

            if (!(a->value = realloc(a->value, size)))
                return M_OUT_OF_MEM;

            memcpy(a->value, value, size);
            a->size = size;
            al->changed = 1;

            return M_OK;
        }
    }

    return M_FAILED;
}

/** 
 * Prepare the given attr_list_t.
 *
 * 'attributes' will point to an array of attribute names
 * that should be in this attribute list. For each element
 * in the array, an attr_t is created in the list.
 *
 * The created attr_t fields will point to the given names
 * in 'attribute' directly and not create copies of them.
 *
 * If the attr list contains any elements already, their 
 * values are freed.
 *
 * M_OK should be returned unless out of memory.
 **/
M_CODE
lm_attrlist_prepare(attr_list_t *al, const char **attributes,
                    unsigned num_attributes)
{
    int x;

    /* free all existing values */
    if (al->num_attributes)
        for (x=0; x<al->num_attributes; x++)
            if (al->list[x].value)
                free(al->list[x].value);
    
    if (num_attributes) {
        if (!(al->list = realloc(al->list, num_attributes*sizeof(attr_t))))
            return M_OUT_OF_MEM;
    } else {
        free(al->list);
        al->list = 0;
    }

    for (x=0; x<num_attributes; x++) {
        al->list[x].name = attributes[x];
        al->list[x].value = 0;
        al->list[x].size = 0;
    }

    al->num_attributes = num_attributes;
    al->changed = 0;

    return M_OK;
}

/** 
 * Free all attribute values and the list 
 **/
void
lm_attrlist_cleanup(attr_list_t *al)
{
    int x;
    if (al->num_attributes) {
        for (x=0; x<al->num_attributes; x++) {
            if (al->list[x].value)
                free(al->list[x].value);
        }
        free(al->list);
    }
}

