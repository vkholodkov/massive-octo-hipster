
#include "trie.h"

boo_trie_t *tree_create(pool_t *pool)
{
    boo_trie_t *t;

    t = pcalloc(pool, sizeof(boo_trie_t));

    if(t == NULL) {
        return NULL;
    }

    t->root = pcalloc(pool, sizeof(boo_trie_node_t));

    if(t->root == NULL) {
        return NULL;
    }

    t->root->leaf = NULL;

    t->pool = pool;

    return t;
}

static boo_trie_node_t*
boo_trie_add_transition(boo_trie_t *tree, boo_trie_node_t *n, boo_uint_t c) {
    boo_trie_node_t           *nn;
    boo_trie_transition_t *nt, *before, *after;

    nn = pcalloc(tree->pool, sizeof(boo_trie_node_t));

    if(nn == NULL) {
        return NULL;
    }

    nn->input_filter = 0;
    nn->from = NULL;
    nn->leaf = NULL;

    nt = pcalloc(tree->pool, sizeof(boo_trie_transition_t));

    if(nt == NULL) {
        return NULL;
    }

    n->input_filter |= bloom_hash(c);

    nt->input = c;
    nt->from = n;
    nt->to = nn;

    if(n->from != NULL) {
        after = NULL;
        before = n->from;

        while(before != NULL) {
            if(before->input > nt->input) {
                break;
            }
            after = before;
            before = before->next;
        }

        if(after == NULL) {
            nt->next = before;
            n->from = nt;
        }
        else {
            nt->next = before;
            after->next = nt;
        }
    }
    else {
        nt->next = NULL;
        n->from = nt;
    }

    return nn;
}

boo_trie_node_t*
boo_trie_add_sequence(boo_trie_t *tree, boo_trie_node_t *node, boo_uint_t *p, boo_uint_t *q) {
    boo_uint_t                c;
    boo_trie_transition_t     *t;
    bloom_filter_t            a;

    while(p != q) {
        c = *p;

        a = bloom_hash(c);

        if((node->input_filter & a) == a) {
            t = node->from;

            while(t != NULL && t->input != c) {
                t = t->next;
            }

            if(t != NULL) {
                node = t->to;
                p++;
                continue;
            }
        }
           
        node = boo_trie_add_transition(tree, node, c);
        p++;
    }

    return node;
}

boo_trie_node_t*
boo_trie_next(boo_trie_t *tree, boo_trie_node_t *node, boo_uint_t c) {
    boo_trie_transition_t *t;
    bloom_filter_t            a;

    a = bloom_hash(c);

    if((node->input_filter & a) == a) {
        t = node->from;

        while(t != NULL && t->input != c) {
            t = t->next;
        }

        if(t != NULL) {
            return t->to;
        }
    }

    return NULL;
}
