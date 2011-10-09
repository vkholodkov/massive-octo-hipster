
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
            printf("%6d,", output->darray->cells[i + base].base);
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
    boo_uint_t i, root;

    root = darray_get_root(output->darray);

    printf("static const boo_uint8 boo_action[] = {\n");

    for(i=0;i != output->max_cells;i++) {
        if(output->darray->cells[i].leaf != 0 &&
            output->darray->cells[i].check != root)
        {
            printf("%6d,", output->darray->cells[i].leaf);
        }
        else {
            printf("%6d,", 0);
        }

        if(i % output->row_stride == output->row_stride - 1) {
            printf("\n");
        }
    }

    printf("\n");

    printf("};\n");

    return BOO_OK;
}

boo_int_t output_check(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i, root, base;

    root = darray_get_root(output->darray);
    base = darray_get_base(output->darray, root);

    printf("static const boo_uint8 boo_check[] = {\n");

    for(i=0;i != output->max_cells;i++) {
        if(output->darray->cells[i].check > 0 &&
            output->darray->cells[i].check != root)
        {
            printf("%6d,", output->darray->cells[i].check - base);
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

boo_int_t output_add_grammar(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t rc;
    boo_uint_t i;
    boo_lalr1_item_set_t *item_set;
    boo_lalr1_item_t *item;
    boo_uint_t root;

    item_set = boo_list_begin(&grammar->item_sets);

    while(item_set != boo_list_end(&grammar->item_sets)) {

        for(i = 0 ; i != grammar->num_symbols ; i++) {
            item = boo_list_begin(&item_set->items);

            while(item != boo_list_end(&item_set->items)) {
                if(item->transition != NULL
                    && boo_token_get(item->rhs[item->pos]) == boo_symbol_to_code(i))
                {
                    rc = darray_insert(output->darray, item_set->state_n, i,
                        item->transition->item_set->state_n);

                    if(rc != BOO_OK) {
                        return rc;
                    }

                    break;
                }
#if 0
                else if(item->pos == item->length) {
                    rc = darray_insert(output->darray, item_set->state_n, i,
                        -23);

                    if(rc != BOO_OK) {
                        return rc;
                    }

                    break;
                }
#endif
                
                item = boo_list_next(item);
            }
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
