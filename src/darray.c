
/*
 * This is simplified double-array implementation of trie.
 * See Aoe 1989 for details.
 *
 */

#include <string.h>
#include <stdio.h>

#include "boo.h"
#include "pool.h"
#include "darray.h"

static boo_int_t darray_find_free_base(darray_t *d, darray_symbols_t *symbols);
static void darray_relocate_base(darray_t *d, darray_symbols_t *symbols, boo_int_t state, boo_int_t new_base);
static boo_int_t darray_expand(darray_t *d);

boo_int_t darray_get_root(darray_t *d) {
    return 2;
}

static boo_int_t darray_get_free_list(darray_t *d) {
    return 1;
}

boo_int_t darray_get_base(darray_t *d, boo_int_t i) {
    return (i < d->ncells) ? d->cells[i].base : BOO_ERROR;
}

boo_int_t darray_get_check(darray_t *d, boo_int_t i) {
    return (i < d->ncells) ? d->cells[i].check : BOO_ERROR;
}

boo_uint_t darray_get_leaf(darray_t *d, boo_int_t i) {
    return (i < d->ncells) ? d->cells[i].leaf : 0;
}

void darray_set_base(darray_t *d, boo_int_t i, boo_int_t val) {
    if (i < d->ncells) {
        d->cells[i].base = val;
    }
}

static void darray_set_check(darray_t *d, boo_int_t i, boo_int_t val) {
    if (i < d->ncells) {
        d->cells[i].check = val;
    }
}

void darray_set_leaf(darray_t *d, boo_int_t i, boo_uint_t leaf) {
    if (i < d->ncells) {
        d->cells[i].leaf = leaf;
    }
}

boo_int_t darray_is_leaf(darray_t *d, boo_int_t i) {
    return darray_get_base(d, i) < 0;
}

static boo_int_t darray_is_free(darray_t *d, boo_int_t i) {
    return darray_get_base(d, i) < 0 && darray_get_check(d, i) < 0;
}

static boo_int_t darray_fits(darray_t *d, boo_int_t state, boo_uint_t c) {
    return state + c < d->ncells;
}

/*
static boo_int_t darray_get_leaf_index(darray_t *d, boo_int_t i) {
    return -darray_get_base(d, i);
}

static void darray_set_leaf_index(darray_t *d, boo_int_t i, boo_int_t val) {
    darray_set_base(d, i, -val);
}
*/

static void darray_alloc_cell(darray_t *d, boo_int_t cell) {
    boo_int_t prev, next;

    prev = -darray_get_base(d, cell);
    next = -darray_get_check(d, cell);

    darray_set_check(d, prev, -next);
    darray_set_base(d, next, -prev);

    d->cells[cell].leaf = 0;
}

static void darray_free_cell(darray_t *d, boo_int_t cell) {
    boo_int_t prev, next;

    prev = darray_get_free_list(d);
    next = -darray_get_check(d, darray_get_free_list(d));

    darray_set_check(d, cell, -next);
    darray_set_base(d, cell, -prev);

    darray_set_base(d, next, -cell);
    darray_set_check(d, prev, -cell);

    d->cells[cell].leaf = 0;
}

darray_t *darray_create(pool_t *pool, boo_uint_t max_symbols) {
    darray_t        *d;
    boo_int_t        i;

    d = palloc(pool, sizeof(darray_t));

    if(d == NULL) {
        return NULL;
    }

    d->pool = pool;

    d->symbols.symbols = palloc(pool, max_symbols * sizeof(boo_uint_t));

    if(d->symbols.symbols == NULL) {
        return NULL;
    }

    d->symbols.num_symbols = 0;

    d->nallocated = DARRAY_INITIAL_LENGTH;
    d->ncells = d->nallocated;
    d->cells = palloc(pool, d->nallocated * sizeof(da_cell_t));

    if(d->cells == NULL) {
        return NULL;
    }

    d->cells[0].base = 0;
    d->cells[0].check = 0;
    d->cells[1].base = -(d->nallocated - 1);
    d->cells[1].check = -3;
    d->cells[2].base = 3;
    d->cells[2].check = 0;

    for(i = 3; i != (d->nallocated - 1); i++) {
        darray_set_check(d, i, -(i + 1));
        darray_set_base(d, i + 1, -i);
    }

    darray_set_check(d, d->nallocated - 1, -1);
    darray_set_base(d, 3, -1);

    return d;
}

boo_int_t darray_walk(darray_t *d, boo_int_t *pi, char c) {
    boo_int_t next;

    next = darray_get_base(d, *pi) + c;

    if(darray_get_check(d, next) == *pi) {
        *pi = next;
        return DARRAY_TRIE_MORE;
    }

    return BOO_OK;
}

static void darray_save_symbols(darray_t *d, darray_symbols_t *symbols, boo_int_t state, boo_int_t base) {
    boo_int_t i;

    symbols->num_symbols = 0;

    for(i = base;i < d->ncells;i++) {
        if(darray_get_check(d, i) == state) {
            symbols->symbols[symbols->num_symbols] = i - base;
            symbols->num_symbols++;
        }
    }
}

boo_int_t darray_insert_branch(darray_t *d, darray_symbols_t *symbols, boo_int_t s, boo_int_t c) {
    boo_int_t       base, next;
    boo_int_t       new_base;

    base = darray_get_base(d, s);

    if(base > 0) {
        next = base + c;

        if(darray_get_check(d, next) == s) {
            return next;
        }

        if(!darray_fits(d, base, c) || !darray_is_free(d, next)) {
            darray_save_symbols(d, symbols, s, base);
            symbols->symbols[symbols->num_symbols++] = c;

            new_base = darray_find_free_base(d, symbols);

            if(new_base == BOO_ERROR) {
                return BOO_ERROR;
            }

            symbols->num_symbols--;

            darray_relocate_base(d, symbols, s, new_base);
            next = new_base + c;
        }
    }
    else {
        symbols->num_symbols = 0;
        symbols->symbols[symbols->num_symbols++] = c;

        new_base = darray_find_free_base(d, symbols);

        if(new_base == BOO_ERROR) {
            return BOO_ERROR;
        }

        darray_set_base(d, s, new_base);
        next = new_base + c;
    }

    darray_alloc_cell(d, next);
    darray_set_check(d, next, s);

    return next;
}

static boo_int_t darray_symbols_fit(darray_t *d, boo_int_t base, darray_symbols_t *symbols) {
    boo_uint_t      i;

    if(base < DARRAY_POOL_BEGIN) {
        return 0;
    }

    for(i=0;i < symbols->num_symbols;i++) {
        if(base + symbols->symbols[i] >= d->ncells) {
            return 0;
        }

        if(!darray_is_free(d, base + symbols->symbols[i])) {
            return 0;
        }
    }

    return 1;
}

static boo_int_t darray_find_free_base(darray_t *d, darray_symbols_t *symbols) {
    char            first_sym;
    boo_int_t       s;

    /* find first free cell that is beyond the first symbol */
    first_sym = symbols->symbols[0];

    s = -darray_get_check(d, darray_get_free_list(d));

    while(s != darray_get_free_list (d)
           && s < (boo_int_t) first_sym + DARRAY_POOL_BEGIN)
    {
        s = -darray_get_check(d, s);
    }

    if(s == darray_get_free_list(d)) {
        do{
            if(darray_expand(d) != BOO_OK) {
                return BOO_ERROR;
            }

            s = -darray_get_check(d, darray_get_free_list(d));
        }while(s == darray_get_free_list(d));
    }

    while(!darray_symbols_fit(d, s - first_sym, symbols)) {
        if(-darray_get_check(d, s) == darray_get_free_list(d)) {
            if(darray_expand(d) != BOO_OK) {
                return BOO_ERROR;
            }
        }

        s = -darray_get_check(d, s);
    }

    return s - first_sym;
}

static boo_int_t darray_expand(darray_t *d) {
    boo_int_t new_begin;
    boo_int_t i;
    boo_int_t free_leaf;
    da_cell_t *old_cells;
    boo_uint_t old_allocated;

    old_cells = d->cells;
    old_allocated = d->nallocated;

    d->nallocated *= 2;

    d->cells = palloc(d->pool, d->nallocated*sizeof(da_cell_t));

    if(d->cells == NULL) {
        return BOO_ERROR;
    }

    memcpy(d->cells, old_cells, old_allocated*sizeof(da_cell_t));

    pfree(d->pool, old_cells);

    new_begin = d->ncells;
    d->ncells = d->nallocated;

    for(i = new_begin + 1; i != d->nallocated; i++) {
        darray_set_check(d, i - 1, -i);
        darray_set_base(d, i, -(i - 1));
    }

    free_leaf = -darray_get_base(d, darray_get_free_list(d));
    darray_set_check(d, free_leaf, -new_begin);
    darray_set_base(d, new_begin, -free_leaf);
    darray_set_check(d, d->nallocated-1, -darray_get_free_list(d));
    darray_set_base(d, darray_get_free_list(d), -(d->nallocated-1));

    d->cells[0].check = d->ncells;

    return BOO_OK;
}

static void darray_relocate_base(darray_t *d, darray_symbols_t *symbols, boo_int_t state, boo_int_t new_base) {
    boo_int_t     old_base;
    boo_int_t     i, j;
    boo_int_t     old_next, new_next, old_next_base;
    boo_uint_t    old_leaf;

    old_base = darray_get_base(d, state);

    for(i = 0; i < symbols->num_symbols; i++) {
        old_next = old_base + symbols->symbols[i];
        new_next = new_base + symbols->symbols[i];
        old_next_base = darray_get_base(d, old_next);
        old_leaf = darray_get_leaf(d, old_next);

        darray_alloc_cell(d, new_next);
        darray_set_check(d, new_next, state);
        darray_set_base(d, new_next, old_next_base);
        darray_set_leaf(d, new_next, old_leaf);

        if(old_next_base > 0) {
            for(j = 0; j < d->ncells; j++) {
                if(darray_get_check(d, old_next_base + j) == old_next) {
                    darray_set_check(d, old_next_base + j, new_next);
                }
            }
        }

        darray_free_cell(d, old_next);
    }

    darray_set_base(d, state, new_base);
}

boo_int_t darray_insert(darray_t *d, boo_uint_t state, boo_uint_t sym, boo_uint_t leaf) {
    boo_int_t       new_node;
    boo_int_t       i;

    i = darray_get_root(d);

    if(darray_walk(d, &i, state) != DARRAY_TRIE_MORE) {
        new_node = darray_insert_branch(d, &d->symbols, i, state);

        if(new_node == BOO_ERROR) {
            return BOO_ERROR;
        }

        darray_set_base(d, new_node, -1);

        i = new_node;
    }

    if(darray_walk(d, &i, sym) != DARRAY_TRIE_MORE) {
        new_node = darray_insert_branch(d, &d->symbols, i, sym);

        if(new_node == BOO_ERROR) {
            return BOO_ERROR;
        }

        darray_set_base(d, new_node, -1);

        i = new_node;
    }

    darray_set_leaf(d, i, leaf);

    return BOO_OK;
}

