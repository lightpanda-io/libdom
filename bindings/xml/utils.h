/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdlib.h>

#ifndef xml_utils_h_
#define xml_utils_h_

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef SLEN
/* Calculate length of a string constant */
#define SLEN(s) (sizeof((s)) - 1) /* -1 for '\0' */
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#ifndef malloc_custom
#define malloc_custom

typedef void *(*custom_m_alloc)(size_t);
typedef void *(*custom_re_alloc)(void*, size_t);
typedef void *(*custom_c_alloc)(size_t, size_t);
typedef void (*custom_f_ree)(void*);

void *m_alloc(size_t size);
void *re_alloc(void *ptr, size_t size);
void *c_alloc(size_t elementCount, size_t size);
void f_ree(void *ptr);

#define malloc(size) m_alloc(size)
#define realloc(ptr, size) re_alloc(ptr, size)
#define calloc(elementCount, size) c_alloc(elementCount, size)
#define free(ptr) f_ree(ptr)

#endif

#endif
