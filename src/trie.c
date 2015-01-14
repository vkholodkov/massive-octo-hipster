
#include <stdio.h>

#include "trie.h"

static boo_int_t trie_add_branch(boo_trie_t *t, boo_int_t node, boo_uint_t *suffix, boo_uint_t *end, boo_uint_t data) {
    boo_int_t       new_node;

    while(suffix != end) {
        new_node = darray_insert_branch(t->darray, &t->darray->symbols, node, *suffix);

        if(new_node == BOO_ERROR) {
            return BOO_ERROR;
        }

        suffix++;

        node = new_node;
    }

    darray_set_base(t->darray, new_node, -1);
    darray_set_leaf(t->darray, new_node, data);
#if 0
    boo_uint_t      l;

    for(l=0;l<t->darray->ncells;l++) {
        if(t->darray->cells[l].base >= 0 || t->darray->cells[l].check >= 0) {
            printf("[%i]:%i %i;", l, t->darray->cells[l].base, t->darray->cells[l].check);
        }
    }
    printf("\n");
#endif
    return BOO_OK;
}

boo_trie_t *trie_create(pool_t *pool) {
    boo_trie_t *t;

    t = palloc(pool, sizeof(boo_trie_t));

    if(t == NULL) {
        return NULL;
    }

    t->pool = pool;
    t->darray = darray_create(pool, 1024);

    if(t->darray == NULL) {
        return NULL;
    }

    return t;
}

boo_int_t trie_insert(boo_trie_t *t, boo_trie_key_t *key, boo_uint_t data) {
    boo_int_t       i;
    boo_uint_t      *p, *q;

    i = darray_get_root(t->darray);

    p = key->data;
    q = p + key->len;

    for(;!darray_is_leaf(t->darray,i) && p != q;p++) {
        if(darray_walk(t->darray, &i, *p) != BOO_TRIE_MORE) {
            return trie_add_branch(t, i, p, q, data);
        }
    }

    if(p != q) {
        return trie_add_branch(t, i, p, q, data);
    }

    /*
     * We've reached a leaf of the trie
     */
    darray_set_leaf(t->darray, i, data);

    return BOO_OK;
}

boo_int_t trie_walker_init(boo_trie_walker_t *w, boo_trie_t *t) {
    w->trie = t;
    w->current = darray_get_root(t->darray);

    return BOO_OK;
}

boo_int_t trie_walker_match(boo_trie_walker_t *w, boo_uint_t sym) {
    return darray_walk(w->trie->darray, &w->current, sym);
}

boo_int_t trie_walker_back(boo_trie_walker_t *w) {
    boo_int_t rc;

    if(w->current == darray_get_root(w->trie->darray)) {
        return BOO_ERROR;
    }

    rc = darray_get_check(w->trie->darray, w->current);

    if(rc == BOO_ERROR) {
        return BOO_ERROR;
    }

    w->current = rc;

    return BOO_OK;
}

boo_uint_t trie_walker_get_leaves(boo_trie_walker_t *w) {
    return darray_get_leaf(w->trie->darray, w->current);
}

