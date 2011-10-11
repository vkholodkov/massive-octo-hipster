
#ifndef _BOO_COMMON_TRIE_
#define _BOO_COMMON_TRIE_

#include "boo.h"
#include "pool.h"
#include "darray.h"

#define BOO_TRIE_MORE       -5

#define BOO_TRIE_WALKER_MAX_STATES      32

typedef struct {
    size_t      len;
    boo_uint_t  *data;
} boo_trie_key_t;

typedef struct {
    pool_t                  *pool;
    darray_t                *darray;
} boo_trie_t;

typedef struct {
    boo_trie_t              *trie;
    boo_int_t               current;
} boo_trie_walker_t;

boo_trie_t *trie_create(pool_t*);
boo_int_t trie_insert(boo_trie_t *t, boo_trie_key_t *key, boo_uint_t data);

boo_int_t trie_walker_init(boo_trie_walker_t*, boo_trie_t*);
boo_int_t trie_walker_match(boo_trie_walker_t*, boo_uint_t);
boo_int_t trie_walker_back(boo_trie_walker_t*);

#endif
