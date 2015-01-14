
#include <stdio.h>

#include "output.h"

boo_output_t *output_create(pool_t *pool)
{
    boo_output_t *output;

    output = pcalloc(pool, sizeof(boo_output_t));

    if(output == NULL) {
        return NULL;
    }

    output->darray = darray_create(pool, 1024);

    if(output->darray == NULL) {
        return NULL;
    }

    output->row_stride = 8;

    return output;
}

#if 0
boo_int_t output_write_translate(output_t *output, grammar_t *grammar)
{
    output_printf(output, "static const boo_uint8 boo_translate[] = {\n");

    for(i = 0 ; i != UCHAR_MAX ; i++) {
        symbol = symtab_resolve(grammar->symtab, token);

        if(symbol != NULL) {
            output_printf(output, "%6d", boo_token_get(symbol->value));
        }
        else {
            output_printf(output, "       0");
        }

        if(i != UCHAR_MAX - 1) {
            output_printf(output, ",");
        }
    }

    output_printf(output, "};\n");
}
#endif

boo_int_t output_base(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t root, base, i;

    root = darray_get_root(output->darray);
    base = darray_get_base(output->darray, root);

    printf("static const boo_uint8 boo_base[] = {\n");

    for(i=0;i != grammar->num_item_sets;i++) {
        if(output->darray->cells[i + base].check == root) {
            printf("%6d,", output->darray->cells[i + base].base - DARRAY_POOL_BEGIN);
        }
        else {
            printf("%6d,", -1);
        }

        if(i % output->row_stride == output->row_stride - 1) {
            printf("\n");
        }
    }

    printf("\n");

    printf("};\n");

    return BOO_OK;
}

boo_int_t output_action(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i, n, root;

    root = darray_get_root(output->darray);

    printf("static const boo_uint8 boo_action[] = {\n");

    n = 0;

    for(i=DARRAY_POOL_BEGIN;i != output->max_cells;i++,n++) {
        if(output->darray->cells[i].check != root)
        {
            printf("%6d,", output->darray->cells[i].leaf);
        }
        else {
            printf("%6d,", 0);
        }

        if(n % output->row_stride == output->row_stride - 1) {
            printf("\n");
        }
    }

    printf("\n");

    printf("};\n");

    return BOO_OK;
}

boo_int_t output_check(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i, n, root, base;

    root = darray_get_root(output->darray);
    base = darray_get_base(output->darray, root);

    printf("static const boo_uint8 boo_check[] = {\n");

    n = 0;

    for(i=DARRAY_POOL_BEGIN;i != output->max_cells;i++,n++) {
        if(output->darray->cells[i].check > 0 &&
            output->darray->cells[i].check != root)
        {
            printf("%6d,", output->darray->cells[i].check - base);
        }
        else {
            printf("%6d,", -1);
        }

        if(n % output->row_stride == output->row_stride - 1) {
            printf("\n");
        }
    }

    printf("\n");

    printf("};\n");

    return BOO_OK;
}

static boo_int_t
output_add_lookahead(boo_output_t *output, boo_grammar_t *grammar, boo_uint_t state, boo_lalr1_item_t *item)
{
    boo_int_t rc;
    boo_trie_t *t;
    boo_trie_node_t *n;
    boo_uint_t *p, *q, symbol, next;
    boo_lhs_lookup_t *lhs_lookup;
    boo_trie_transition_t *tr;

    t = grammar->lookahead_set;

    n = boo_trie_next(t, t->root, item->lhs);

    if(n == NULL) {
        return BOO_OK;
    }

    p = item->rhs;
    q = p + item->length;

    while(p != q) {
        symbol = boo_token_get(*p);

        n = boo_trie_next(t, n, symbol);

        if(n == NULL) {
            return BOO_OK;
        }

        p++;
    }

    tr = n->to;

    while(tr != NULL) {
        if(tr->input != BOO_EOF) {
            symbol = boo_code_to_symbol(tr->input);
            lhs_lookup = grammar->lhs_lookup + symbol;
            next = tr->to->leaf;

            if(lhs_lookup->literal) {
                symbol = lhs_lookup->name.data[0];
                printf("Adding lookahead '%c' to state %d -> %d\n", symbol, state, next);
            }
            else {
                printf("Adding lookahead %d to state %d -> %d\n", symbol, state, next);
            }
        }
        else {
            symbol = 0;
            printf("Adding lookahead $eof to state %d -> %d\n", state, next);
        }

        rc = darray_insert(output->darray, state, symbol, -item->rule_n);
 
        if(rc != BOO_OK) {
            return rc;
        }

        tr = tr->next;
    }

    return BOO_OK;
}

boo_int_t output_add_grammar(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t rc;
    boo_uint_t i, symbol;
    boo_lalr1_item_set_t *item_set;
    boo_lalr1_item_t *item;
    boo_uint_t root;
    boo_lhs_lookup_t *lhs_lookup;

    item_set = boo_list_begin(&grammar->item_sets);

    while(item_set != boo_list_end(&grammar->item_sets)) {

        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {
            if(item->transition != NULL)
            {
                if(boo_is_token(item->rhs[item->pos])) {
                    symbol = boo_code_to_symbol(item->rhs[item->pos]);
                    lhs_lookup = grammar->lhs_lookup + symbol;

                    if(lhs_lookup->literal) {
                        symbol = lhs_lookup->name.data[0];

                        printf("Adding transition '%c' to state %d -> %d\n",
                            symbol, item_set->state_n,
                            item->transition->item_set->state_n);
                    }
                    else {
                        printf("Adding transition %d to state %d -> %d\n",
                            symbol, item_set->state_n,
                            item->transition->item_set->state_n);
                    }

                    rc = darray_insert(output->darray, item_set->state_n,
                        boo_token_get(item->rhs[item->pos]),
                        item->transition->item_set->state_n);

                    if(rc != BOO_OK) {
                        return rc;
                    }
                }
            }
            else if(item->pos == item->length) {
                rc = output_add_lookahead(output, grammar, item_set->state_n, item);

                if(rc != BOO_OK) {
                    return rc;
                }
            }
            else {
                rc = darray_insert(output->darray, item_set->state_n,
                    boo_token_get(item->rhs[item->pos]), 0);

                if(rc != BOO_OK) {
                    return rc;
                }
            }
            
            item = boo_list_next(item);
        }

        item_set = boo_list_next(item_set);
    }

    root = darray_get_root(output->darray);

    for(i=0;i != output->darray->ncells;i++) {
        if((output->darray->cells[i].base >= 0 ||
            output->darray->cells[i].check >= 0) &&
            output->darray->cells[i].check != root)
        {
            output->max_cells = i + 1;
        }
    }

    return BOO_OK;
}
