
#ifndef _TREE_H_
#define _TREE_H_

#include "boo.h"
#include "pool.h"

typedef boo_uint_t bloom_filter_t;
typedef boo_uint_t stateid_t;

#define my_hash(base, value) (((value) % (base)) & 0x1f)
#define bloom_hash(value) ( (1 << my_hash(7, (value))) | (1 << my_hash(13, (value))) | (1 << my_hash(17, (value))) )

typedef struct boo_trie_leaf {
    struct boo_trie_leaf    *next;
} boo_trie_leaf_t;

struct boo_trie_transition;

typedef struct {
    bloom_filter_t                   input_filter;

    /*
     * Linked lists of transitions from this node
     */
    struct boo_trie_transition       *from;
    void                             *leaf;
} boo_trie_node_t;

typedef struct boo_trie_transition {
    boo_uint_t                     input;
    boo_trie_node_t                *from, *to;
    struct boo_trie_transition     *next;
} boo_trie_transition_t;

typedef struct {
    pool_t                      *pool;
    boo_trie_node_t             *root;

    boo_uint_t                  end;
    boo_uint_t                  num_states;
} boo_trie_t;

boo_trie_t *trie_create(pool_t*);
boo_trie_node_t *boo_trie_add_sequence(boo_trie_t*, boo_trie_node_t*, boo_uint_t*, boo_uint_t*);
boo_trie_node_t *boo_trie_next(boo_trie_t*, boo_trie_node_t*, boo_uint_t);

#endif
