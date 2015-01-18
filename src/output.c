
#include <stdio.h>

#include "output.h"

boo_output_t *output_create(pool_t *pool)
{
    boo_output_t *output;

    output = pcalloc(pool, sizeof(boo_output_t));

    if(output == NULL) {
        return NULL;
    }

    output->pool = pool;
    output->row_stride = 8;

    return output;
}

boo_int_t output_codes(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i;

    fprintf(output->file, "#define BOO_EOF %d\n", UCHAR_MAX + 1);

    for(i = 0 ; i != grammar->num_symbols ; i++) {
        if(grammar->lhs_lookup[i].token && !grammar->lhs_lookup[i].literal) {
            fprintf(output->file, "#define BOO_");
            boo_puts(output->file, &grammar->lhs_lookup[i].name);
            fprintf(output->file, " %d\n", grammar->lhs_lookup[i].code);
        }
    }

    fprintf(output->file, "\n");

    return BOO_OK;
}

boo_int_t output_symbols(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i;

    fprintf(output->file, "static const char *boo_symbol[] = {\n      ");

    for(i = 0 ; i != grammar->num_symbols ; i++) {
        fprintf(output->file, "\"");
        boo_escape_puts(output->file, &grammar->lhs_lookup[i].name);
        fprintf(output->file, "\", ");

        if(i % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n      ");
        }
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n\n");

    return BOO_OK;
}

boo_int_t output_lookup(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t rc = lookup_write(output->file, output->term, "boo_t");

    if(rc != BOO_OK) {
        return rc;
    }

    return lookup_write(output->file, output->nterm, "boo_nt");
}

boo_int_t output_actions(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i, n = 0;
    boo_reduction_t *reduction;

    fprintf(output->file, "static const boo_uint_t boo_action[] = {\n");

    reduction = boo_list_begin(&grammar->reductions);

    while(reduction != boo_list_end(&grammar->reductions)) {
        fprintf(output->file, "%6d,", reduction->num_actions);

        if(n % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }

        n++;

        for(i = 0 ; i != reduction->num_actions ; i++) {
            fprintf(output->file, "%6d,", reduction->actions[i]);

            if(n % output->row_stride == output->row_stride - 1) {
                fprintf(output->file, "\n");
            }

            n++;
        }

        reduction = boo_list_next(reduction);
    }

    fprintf(output->file, "\n};\n\n");

    return BOO_OK;
}

boo_int_t output_lhs(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t n = 0;
    boo_rule_t *rule;

    fprintf(output->file, "static const boo_uint_t boo_lhs[] = {\n");

    rule = boo_list_begin(&grammar->rules);

    while(rule != boo_list_end(&grammar->rules)) {
        fprintf(output->file, "%6d,", boo_code_to_symbol(rule->lhs));

        if(n % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }

        n++;

        rule = boo_list_next(rule);
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n\n");

    return BOO_OK;
}

boo_int_t output_rhs(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t i, n = 0;
    boo_rule_t *rule;

    fprintf(output->file, "static const boo_uint_t boo_rhs[] = {\n");

    rule = boo_list_begin(&grammar->rules);

    while(rule != boo_list_end(&grammar->rules)) {
        for(i = 0 ; i != rule->length ; i++) {
            fprintf(output->file, "%6d,", boo_code_to_symbol(rule->rhs[i]));

            if(n % output->row_stride == output->row_stride - 1) {
                fprintf(output->file, "\n");
            }

            n++;
        }

        rule = boo_list_next(rule);
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n\n");

    return BOO_OK;
}

boo_int_t output_rules(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t n = 0, pos = 0;
    boo_rule_t *rule;

    fprintf(output->file, "static const boo_uint_t boo_rule[] = {\n");

    rule = boo_list_begin(&grammar->rules);

    while(rule != boo_list_end(&grammar->rules)) {
        fprintf(output->file, "%6d,", rule->length);

        if(n % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }

        n++;

        fprintf(output->file, "%6d,", pos);

        if(n % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }

        n++;

        pos += rule->length;

        rule = boo_list_next(rule);
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n\n");

    return BOO_OK;
}

static void
output_renumber_reduction(boo_grammar_t *grammar)
{
    boo_reduction_t *reduction;
    boo_uint_t pos = 0;

    reduction = boo_list_begin(&grammar->reductions);

    while(reduction != boo_list_end(&grammar->reductions)) {
        reduction->pos = pos;
        pos += (1 /* number of reductions */ + reduction->num_actions);
        reduction = boo_list_next(reduction);
    }
}

static boo_int_t
output_add_lookahead(boo_output_t *output, boo_grammar_t *grammar, boo_uint_t state, boo_lalr1_item_t *item)
{
    boo_int_t rc;
    boo_trie_t *t;
    boo_trie_node_t *n;
    boo_uint_t *p, *q, symbol;
    boo_lhs_lookup_t *lhs_lookup;
    boo_trie_transition_t *tr;
    boo_reduction_t *reduction;

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
        if(tr->to->leaf != NULL) {
            reduction = tr->to->leaf;

            if(tr->input != BOO_EOF) {
                symbol = boo_code_to_symbol(tr->input);
                lhs_lookup = grammar->lhs_lookup + symbol;

                if(lhs_lookup->literal) {
                    symbol = lhs_lookup->name.data[0];
                    fprintf(output->debug, "adding lookahead '%c' to state %d reduce %d\n",
                        symbol, state, reduction->actions[reduction->num_actions - 1]);
                }
                else {
                    symbol = lhs_lookup->code;
                    fprintf(output->debug, "adding lookahead ");
                    boo_puts(output->debug, &lhs_lookup->name);
                    fprintf(output->debug, " (%d) to state %d reduce %d\n", symbol,
                        state, reduction->actions[reduction->num_actions - 1]);
                }
            }
            else {
                symbol = UCHAR_MAX + 1;
                fprintf(output->debug, "adding lookahead $eof to state %d accept\n", state);
            }

            rc = lookup_add_transition(output->term, state, symbol, -reduction->pos);
     
            if(rc != BOO_OK) {
                return rc;
            }
        }

        tr = tr->next;
    }

    return BOO_OK;
}

boo_int_t output_add_grammar(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t rc;
    boo_uint_t symbol;
    boo_lalr1_item_set_t *item_set;
    boo_lalr1_item_t *item;
    boo_lhs_lookup_t *lhs_lookup;
    boo_uint_t i, code;

    code = UCHAR_MAX + 2;

    for(i = 0 ; i != grammar->num_symbols ; i++) {
        if(grammar->lhs_lookup[i].token && !grammar->lhs_lookup[i].literal) {
            grammar->lhs_lookup[i].code = code;
            code++;
        }
    }

    output->term = lookup_create(output->pool, grammar->num_item_sets);

    if(output->term == NULL) {
        return BOO_ERROR;
    }

    output->term->debug = output->debug;

    output->nterm = lookup_create(output->pool, grammar->num_item_sets);

    if(output->nterm == NULL) {
        return BOO_ERROR;
    }

    output->nterm->debug = output->debug;

    output_renumber_reduction(grammar);

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

                        fprintf(output->debug, "adding transition '%c' to state %d -> %d\n",
                            symbol, item_set->state_n,
                            item->transition->item_set->state_n);
                    }
                    else {
                        symbol = lhs_lookup->code;

                        fprintf(output->debug, "adding transition ");
                        boo_puts(output->debug, &lhs_lookup->name);
                        fprintf(output->debug, " (%d) to state %d -> %d\n",
                            symbol, item_set->state_n,
                            item->transition->item_set->state_n);
                    }

                    rc = lookup_add_transition(output->term, item_set->state_n, symbol,
                        item->transition->item_set->state_n);

                    if(rc != BOO_OK) {
                        return rc;
                    }
                }
                else /*if(item->core)*/ {
                    rc = lookup_add_transition(output->nterm, item_set->state_n,
                        boo_code_to_symbol(item->rhs[item->pos]),
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
            else if(boo_token_get(item->rhs[item->pos]) == BOO_EOF) {
                rc = lookup_add_transition(output->term, item_set->state_n,
                    256, 0);

                if(rc != BOO_OK) {
                    return rc;
                }
            }
            
            item = boo_list_next(item);
        }

        item_set = boo_list_next(item_set);
    }

    rc = lookup_index(output->term);

    if(rc != BOO_OK) {
        return rc;
    }

    return lookup_index(output->nterm);
}
