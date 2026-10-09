/*
 * Configuration of the vendored libical for this firmware (and for the host tests, glibc).
 *
 * It replaces the config.h that libical's own CMake build generates for a given system: only what
 * the sources in ../libical actually use is defined, for a single-threaded, little-endian, 32-bit
 * target with a 64-bit time_t (ESP-IDF's newlib) and no file system or time zone database access.
 * Not part of upstream.
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR MPL-2.0 (the same as libical; this file is ours)
 */
#ifndef LIBICAL_PORT_CONFIG_H
#define LIBICAL_PORT_CONFIG_H

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wctype.h>

/* libical's own time_t substitutes (icalendar times are always time_t here) */
#define SIZEOF_TIME_T 8
#define SIZEOF_ICALTIME_T 8
#ifndef __cplusplus
_Static_assert(sizeof(time_t) == SIZEOF_TIME_T, "libical port: time_t must be 64 bit");
#endif
#define SIZEOF_INT 4
#define SIZEOF_UCHAR 1

#define icaltime(timer) time(timer)
#define icalctime(timer) ctime(timer)
#define icalmktime(timeptr) mktime(timeptr)
#define icalgmtime_r(timer, buf) gmtime_r(timer, buf)
#define icallocaltime_r(timer, buf) localtime_r(timer, buf)

/* Single task, no pthreads: the global variables of libical stay plain globals. Whoever calls into
 * libical must not do so from two tasks at once (calendar_rrule.c serializes with a mutex). The
 * sources compare ICAL_SYNC_MODE with these values; left undefined, both sides read as 0 and
 * "pthread mode" is chosen. */
#define ICAL_SYNC_MODE_NONE 1
#define ICAL_SYNC_MODE_PTHREAD 2
#define ICAL_SYNC_MODE_THREADLOCAL 3
#define ICAL_SYNC_MODE ICAL_SYNC_MODE_NONE
#define ICAL_GLOBAL_VAR

#define ICALMEMORY_DEFAULT_MALLOC malloc
#define ICALMEMORY_DEFAULT_REALLOC realloc
#define ICALMEMORY_DEFAULT_FREE free

/* An internal invariant of libical that does not hold is a bug, not an input error: stop.
 * (calendar_rrule.c only hands libical rules it has checked itself, and the host fuzzer exercises
 * exactly that.) */
#define icalassert(...) assert(__VA_ARGS__)

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#define _unused(x) (void) x
#if defined(__GNUC__) || defined(__clang__)
#define _fallthrough() __attribute__((fallthrough))
#else
#define _fallthrough() (void) 0
#endif

/* no time zone files, no backtraces, no ICU */
#define PACKAGE_DATA_DIR "/"

#endif
