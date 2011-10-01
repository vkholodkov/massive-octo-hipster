
#ifndef _BOO_COMMON_VECTOR_
#define _BOO_COMMON_VECTOR_

#include "boo.h"
#include "pool.h"

typedef boo_int_t (*boo_comparator_f)(void*, void*);

typedef struct {
    pool_t              *pool;
    size_t              element_size;
    boo_uint_t          allocated;
    boo_uint_t          nelements;
    void                *elements;
} boo_vector_t;

boo_vector_t *vector_create(pool_t*, size_t, boo_uint_t);
void *vector_append(boo_vector_t*);
boo_int_t vector_insert(boo_vector_t*, void*, void*);
boo_int_t vector_remove(boo_vector_t*, void*);
void vector_clear(boo_vector_t*);
void *vector_lower_bound(boo_vector_t*, void*, boo_comparator_f);

#endif //_BOO_COMMON_VECTOR_
