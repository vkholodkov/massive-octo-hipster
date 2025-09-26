
#include <string.h>

#include "vector.h"

boo_vector_t *vector_create(pool_t *pool, size_t element_size, boo_uint_t preallocate) {
    boo_vector_t *v;

    v = palloc(pool, sizeof(boo_vector_t));

    if(v == NULL) {
        return NULL;
    }

    v->pool = pool;
    v->element_size = element_size;
    v->allocated = preallocate;
    v->nelements = 0;

    v->elements = palloc(v->pool, element_size * preallocate);

    if(v->elements == NULL) {
        return NULL;
    }

    return v;
}

static boo_int_t vector_grow(boo_vector_t *v) {
    void *old_elements;

    v->allocated *= 2;

    old_elements = v->elements;

    v->elements = palloc(v->pool, v->allocated * v->element_size);

    if(v->elements == NULL) {
        return BOO_ERROR;
    }

    memcpy(v->elements, old_elements, v->nelements * v->element_size);

    pfree(v->pool, old_elements);

    return BOO_OK;
}

boo_int_t vector_insert(boo_vector_t *v, void *p, void *data) {
    u_char *last, *next;

    if(v->nelements + 1 == v->allocated) {
        return BOO_ERROR;
    }

    last = ((u_char*)v->elements) + v->nelements * v->element_size;

    if(p < v->elements || (u_char*)p > last) {
        return BOO_ERROR;
    }

    if(p != last) {
        next = ((u_char*)p) + v->element_size;
        memmove(next, p, last - (u_char*)p);
    }

    memcpy(p, data, v->element_size);

    v->nelements++;

    return BOO_OK;
}

void *vector_append(boo_vector_t *v) {
    u_char *last;

    if(v->nelements + 1 == v->allocated) {
        if(vector_grow(v) != BOO_OK) {
            return NULL;
        }
    }

    last = ((u_char*)v->elements) + v->nelements * v->element_size;

    v->nelements++;

    return last;
}

boo_int_t vector_remove(boo_vector_t *v, void *p) {
    u_char *last, *next;

    if(v->nelements < 1) {
        return BOO_ERROR;
    }

    last = ((u_char*)v->elements) + (v->nelements - 1) * v->element_size;

    if(p < v->elements || (u_char*)p > last) {
        return BOO_ERROR;
    }

    if(p != last) {
        next = ((u_char*)p) + v->element_size;
        memmove(p, next, last - next);
    }

    v->nelements--;

    return BOO_OK;
}

void vector_clear(boo_vector_t *v) {
    v->nelements = 0;
}

void *vector_lower_bound(boo_vector_t *v, void *data, boo_comparator_f comparator) {
    u_char *p, *first;
    size_t count, step;

    first = v->elements;
    count = v->nelements;

    while(count > 0) {
        p = first;
        step = count / 2;
        p += step * v->element_size;

        if(comparator(p, data) < 0) {
            first = p + v->element_size;
            p += v->element_size;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }

    return first;
}
