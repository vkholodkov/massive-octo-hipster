
#ifndef _BOO_DARRAY_H_
#define _BOO_DARRAY_H_

#define DARRAY_MAX_SYMBOLS            1024

#define DARRAY_INITIAL_LENGTH       100
#define DARRAY_POOL_BEGIN           3

#define DARRAY_TRIE_MORE       -5

typedef struct {
    boo_uint_t          num_symbols;
    boo_uint_t          *symbols;
} darray_symbols_t;

typedef struct {
    boo_int_t           base;
    boo_int_t           check;
    boo_uint_t          leaf;
} da_cell_t;

typedef struct {
    pool_t              *pool;
    boo_int_t           ncells;
    boo_int_t           nallocated;
    da_cell_t           *cells;
    darray_symbols_t    symbols;
} darray_t;

darray_t *darray_create(pool_t*, boo_uint_t);
boo_int_t darray_insert(darray_t*, boo_uint_t, boo_uint_t, boo_uint_t);
boo_int_t darray_get_root(darray_t*);
boo_int_t darray_get_base(darray_t*, boo_int_t);

#endif
