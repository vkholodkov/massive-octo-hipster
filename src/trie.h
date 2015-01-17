
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
     * Linked lists of transitions from and to this state
     */
    struct boo_trie_transition       *to;

//    boo_trie_leaf_t                  *leaf;
    boo_int_t                       leaf;
} boo_trie_node_t;

typedef struct boo_trie_transition {
    boo_uint_t                     input;
    boo_trie_node_t                *from, *to;
    struct boo_trie_transition     *next;
} boo_trie_transition_t;

#if 0
typedef struct boo_http_waf_gap {
    boo_uint_t from, to;
    struct boo_http_waf_gap *next;
} boo_http_waf_gap_t;

typedef struct {
    boo_uint_t                  base;
    boo_uint_t                  mask;
    boo_http_waf_leaf_t         *leaf;
    boo_uint_t                  padding;
} boo_http_waf_state_t;
#endif

typedef struct {
    pool_t                      *pool;
    boo_trie_node_t             *root;
#if 0
    boo_http_waf_gap_t          *gaps, *free_gaps;
#endif

    boo_uint_t                  end;
    boo_uint_t                  num_states;
#if 0
    boo_http_waf_state_t        *states;
    u_char                      *next;
#endif

} boo_trie_t;

boo_trie_t *tree_create(pool_t*);
boo_trie_node_t *boo_trie_add_sequence(boo_trie_t*, boo_trie_node_t*, boo_uint_t*, boo_uint_t*);
boo_trie_node_t *boo_trie_next(boo_trie_t*, boo_trie_node_t*, boo_uint_t);

#if 0
boo_int_t boo_http_waf_index_nodes(boo_http_waf_tree_t*);
#endif

#endif
