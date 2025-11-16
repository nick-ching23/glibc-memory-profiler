#ifndef _MALLOC_PROF_H
#define _MALLOC_PROF_H

#include <stddef.h>

void __mp_on_alloc(size_t size, void *ptr);

#endif