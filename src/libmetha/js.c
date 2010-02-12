/*-
 * js.c
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

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "js.h"
#include "worker.h"
#include "io.h"

static JSBool __lm_js_print(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_println(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_getc(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_get(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_http_post(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_filesize(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_set_attribute(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool __lm_js_exec(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);

JSFunctionSpec lm_js_allfunctions[] = {
    {"print", __lm_js_print, 1},
    {"println", __lm_js_println, 1},
    {"getc", __lm_js_getc, 0},
    {"get", __lm_js_get, 1},
    {"http_post", __lm_js_http_post, 2},
    {"filesize", __lm_js_filesize, 1},
    {"exec", __lm_js_exec, 1},
    {0}
};

/** 
 * functions usable on the 'this' object 
 * of workers. i.e. this.set_attribute("x", "y");
 **/
JSFunctionSpec lm_js_workerfunctions[] = {
    {"set_attribute", __lm_js_set_attribute, 2},
    {0}
};

void
lm_jserror(JSContext *cx, const char *message, JSErrorReport *report)
{
    char *p;

    if (report->filename) {
        if (!(p = strrchr(report->filename, '/')))
            p = (char*)report->filename;
        else 
            p++;

        fprintf(stderr, "<%s:%u>: %s\n", p, (unsigned)report->lineno, message);
    } else 
        fprintf(stderr, "<?:%d>: %s\n", report->lineno, message);
}

/** 
 * Call a function for each value found in the given jsval. If the 
 * jsval is an array, the function will be called for each element
 * in the array.
 *
 * 'cx' is the spidermonkey context to work within, this should be 
 * the context used when the jsval was created (i.e. when the 
 * javascript function was called).
 **/
M_CODE
lm_jsval_foreach(JSContext *cx, jsval v,
                 M_CODE (*__callback)(void *, const char *, uint16_t),
                 void *extra)
{
    int32_t sz, x;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
    } else if (JSVAL_IS_OBJECT(v)) {
        jsval length;
        JSObject *o;

        JS_ValueToObject(cx, v, &o);

        if (JS_IsArrayObject(cx, o)) {
            jsuint len;
            JS_GetArrayLength(cx, o, &len);
            sz = (int32_t)len;
        } else {
            JS_CallFunctionName(cx, o, "length", 0, 0, &length);
            JS_ValueToInt32(cx, length, &sz);
        }
            
        jsval tmp;

        for (x=0; x<sz; x++) {
            if (JS_GetElement(cx, o, x, &tmp) == JS_TRUE) {
                JSString *retstring = JS_ValueToString(cx, tmp);
                char *retc = JS_GetStringBytes(retstring);
                __callback(extra, retc, (int)JS_GetStringLength(retstring));
            }
        }
    } else {
        JSString *retstring = JS_ValueToString(cx, v);
        char *retc = JS_GetStringBytes(retstring);
        __callback(extra, retc, (int)JS_GetStringLength(retstring));
    }

    return M_OK;
}

JSBool
__lm_js_print(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    const char *out;

    if (!JS_ConvertArguments(cx, argc, argv, "s", &out))
        return JS_FALSE;

    printf("%s", out);

    return JS_TRUE;
}

JSBool
__lm_js_println(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    const char *out;

    if (!JS_ConvertArguments(cx, argc, argv, "s", &out))
        return JS_FALSE;

    printf("%s\n", out);

    return JS_TRUE;
}

JSBool
__lm_js_getc(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    char out[] = "_";
    JSString *tmp;

    out[0] = getc(stdin);
    tmp = JS_NewStringCopyN(cx, out, 1);

    *ret = STRING_TO_JSVAL(tmp);

    return JS_TRUE;
}

/** 
 *
 **/
static JSBool
__lm_js_get(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    CURL *h;
    CURLcode c;
    iobuf_t b;
    JSString *tmp;
    const char *url;

    b.sz = 0;
    b.cap = 1024;

    if (argc != 1)
        return JS_FALSE;

    if (!(b.ptr = malloc(b.cap)))
        return JS_FALSE;
    h = curl_easy_init();

    if (!JS_ConvertArguments(cx, argc, argv, "s", &url))
        return JS_FALSE;

    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &lm_io_data_cb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &b);

    if ((c = curl_easy_perform(h)) != CURLE_OK) {
        *ret = JSVAL_FALSE;
        fprintf(stderr, "%s\n", curl_easy_strerror(c));
    } else {
        tmp = JS_NewStringCopyN(cx, b.ptr, b.sz);
        *ret = STRING_TO_JSVAL(tmp);
    }

    free(b.ptr);
    curl_easy_cleanup(h);

    return JS_TRUE;
}

/** 
 * Perform an HTTP post using the given arguments, return the HTTP response. 
 **/
JSBool
__lm_js_http_post(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    const char *url;
    const char *arguments;
    JSString   *tmp;

    if (!JS_ConvertArguments(cx, argc, argv, "ss", &url, &arguments))
        return JS_FALSE;

    if (!url || !arguments)
        return JS_FALSE;

    CURL *h_post;
    CURLcode c;
    iobuf_t b;
    b.sz = 0;
    b.cap = 1024;
    if (!(b.ptr = malloc(b.cap)))
        return JS_FALSE;
    h_post = curl_easy_init();

    curl_easy_setopt(h_post, CURLOPT_URL, url);
    curl_easy_setopt(h_post, CURLOPT_WRITEFUNCTION, &lm_io_data_cb);
    curl_easy_setopt(h_post, CURLOPT_POSTFIELDS, arguments);
    curl_easy_setopt(h_post, CURLOPT_WRITEDATA, &b);

    if ((c = curl_easy_perform(h_post)) != CURLE_OK)
        fprintf(stderr, "%s\n", curl_easy_strerror(c));

    tmp = JS_NewStringCopyN(cx, b.ptr, b.sz);
    *ret = STRING_TO_JSVAL(tmp);

    free(b.ptr);
    curl_easy_cleanup(h_post);

    return JS_TRUE;
}

/** 
 * Attempt to open the file given in the first argument and determine its size
 **/
JSBool
__lm_js_filesize(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    const char *filename;
    long filesize;

    if (!JS_ConvertArguments(cx, argc, argv, "s", &filename))
        return JS_FALSE;

    if (!filename)
        return JS_FALSE;

    FILE *fh;
    fh = fopen(filename, "r");

    if (fh == NULL) {
        fprintf(stderr, "error opening file %s\n", filename);
        return JS_FALSE;
    }

    fseek(fh, 0, SEEK_END);
    filesize = ftell(fh);
    fclose(fh);

    *ret = INT_TO_JSVAL(filesize);
    return JS_TRUE;
}

/** 
 * set the given filetype attribute of the
 * current URL
 **/
JSBool
__lm_js_set_attribute(JSContext *cx, JSObject *this,
                 uintN argc, jsval *argv,
                 jsval *ret)
{
    worker_t *w;
    const char *attr, *val;

    if (!(w = JS_GetPrivate(cx, this)))
        return JS_FALSE;

    if (argc != 2) {
        *ret = JSVAL_FALSE;
        return JS_TRUE;
    }

    if (!JS_ConvertArguments(cx, argc, argv, "ss", &attr, &val))
        return JS_FALSE;

    if (lm_attribute_set(&w->attributes, attr, val, strlen(val)) == M_OK)
        *ret = JSVAL_TRUE;
    else
        *ret = JSVAL_FALSE;

    return JS_TRUE;
}

#define BUFSZ 512
/** 
 * execute the given command, return the 
 * output of the command
 **/
static JSBool
__lm_js_exec(JSContext *cx, JSObject *this, uintN argc,
             jsval *argv, jsval *ret)
{
    FILE *fp;
    char *cmd;
    char *buf = 0;
    size_t n, sz = 0;
    JSString *str;

    if (!argc) {
        *ret = JSVAL_FALSE;
        return JS_TRUE;
    }

    str = JS_ValueToString(cx, argv[0]);
    cmd = JS_GetStringBytes(str);

    if ((fp = popen(cmd, "r"))) {
        do {
            if (!(buf = JS_realloc(cx, buf, sz+BUFSZ))) {
                pclose(fp);
                return JS_FALSE;
            }
            sz += (n = (size_t)fread(buf+sz, 1, BUFSZ, fp));
        } while (n);

        pclose(fp);

        if (!sz) {
            /* no data, we'll return an empty string */
            JS_free(cx, buf);
            *ret = JS_GetEmptyStringValue(cx);
        } else {
            /* free wasted space */
            if (!(buf = JS_realloc(cx, buf, sz+1)))
                return JS_FALSE;
            buf[sz] = '\0'; /* the documentation said NOTHING about this,
                             * why do I have to null-terminate AND provide the length? 
                             * plain stupid mozilla-shit */
            if (!(str = JS_NewString(cx, buf, sz))) {
                JS_free(cx, buf);
                return JS_FALSE;
            }
            *ret = STRING_TO_JSVAL(str);
        }
    } else
        *ret = JSVAL_FALSE;

    return JS_TRUE;
}

