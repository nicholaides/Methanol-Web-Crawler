/* src/config.h.  Generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* used by modules to build with the same flags at libmetha */
#define BUILD_CFLAGS "-I/usr/include/js -D_GNU_SOURCE -DJS_THREADSAFE -DXP_UNIX "

/* used by modules to link the same libraries as libmetha */
#define BUILD_LIBS "-lpthread -lcurl -ljs -lev -lmysqlclient_r "

/* Define 1 if your version of gcc supports __sync_add_and_fetch */
#define HAVE_BUILTIN_ATOMIC 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `epoll_ctl' function. */
#define HAVE_EPOLL_CTL 1

/* Define to 1 if you have the <ev.h> header file. */
#define HAVE_EV_H 1

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `curl' library (-lcurl). */
#define HAVE_LIBCURL 1

/* Define to 1 if you have the `ev' library (-lev). */
#define HAVE_LIBEV 1

/* Define to 1 if you have the `js' library (-ljs). */
#define HAVE_LIBJS 1

/* Define to 1 if you have the `mozjs' library (-lmozjs). */
/* #undef HAVE_LIBMOZJS */

/* Define to 1 if you have the `mysqlclient_r' library (-lmysqlclient_r). */
#define HAVE_LIBMYSQLCLIENT_R 1

/* Define to 1 if you have the `nspr4' library (-lnspr4). */
/* #undef HAVE_LIBNSPR4 */

/* Define to 1 if you have the `wsock32' library (-lwsock32). */
/* #undef HAVE_LIBWSOCK32 */

/* Define to 1 if you have the `memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <mysql.h> header file. */
#define HAVE_MYSQL_H 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `realloc' function. */
#define HAVE_REALLOC 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the <sys/epoll.h> header file. */
#define HAVE_SYS_EPOLL_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "methanol"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://metha-sys.org/"

/* Define to the full name of this package. */
#define PACKAGE_NAME "methanol"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "methanol 1.7.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "methanol"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.7.0"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.7.0"
