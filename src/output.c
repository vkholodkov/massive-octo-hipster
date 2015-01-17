
#include <stdio.h>

#include "output.h"

#define boo_trans_len (2 * sizeof(boo_int_t))

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

static boo_gap_t *output_alloc_gap(boo_output_t *output) {
    boo_gap_t *g;

    if(output->free_gaps != NULL) {
        g = output->free_gaps;
        output->free_gaps = g->next;
        g->next = NULL;
        return g;
    }

    g = pcalloc(output->pool, sizeof(boo_output_t));

    if(g == NULL) {
        return NULL;
    }

    return g;
}

static void output_free_gap(boo_output_t *output, boo_gap_t *g) {
    g->from = g->to = 0;
    g->next = output->free_gaps;
    output->free_gaps = g;
}

static void output_add_gap(boo_output_t *output, boo_gap_t *g)
{
    boo_gap_t *after, *before;

    if(output->gaps != NULL) {
        after = NULL;
        before = output->gaps;

        while(before != NULL) {
            if(before->from >= g->to) {
                break;
            }
            after = before;
            before = before->next;
        }

        if(after == NULL) {
            g->next = before;
            output->gaps = g;
        }
        else {
            g->next = before;
            after->next = g;
        }
    }
    else {
        g->next = NULL;
        output->gaps = g;
    }

    fprintf(output->debug, "add gap <%u, %u>\n", g->from, g->to);
}

static boo_int_t
output_fill_gap(boo_output_t *output, boo_uint_t from, boo_uint_t to) {
    boo_gap_t *p, *next, *prev;
    boo_uint_t p_from, p_to;
    boo_gap_t *new_gaps = NULL, *new_gap;

    fprintf(output->debug, "fill_gap <%u, %u>\n", from, to);

    p = output->gaps;
    prev = NULL;

    while(p != NULL) {
        next = p->next;

        if((p->from < to) && (p->to > from)) {
            p_from = p->from >= from ? p->from : from;
            p_to = p->to <= to ? p->to : to;

            if(p->from == p_from && p->to == p_to) {
                p->from = p->to;
            }
            else if(p->from == from) {
                p->from = p_to;
            }
            else if(p->to == to) {
                p->to = p_from;
            }
            else {
                new_gap = output_alloc_gap(output);

                if(new_gap == NULL) {
                    return BOO_ERROR;
                }

                new_gap->from = p_to;
                new_gap->to = p->to;
                new_gap->next = new_gaps;
                new_gaps = new_gap;

                p->to = p_from;
            }

            if(p->from == p->to) {
                if(p == output->gaps) {
                    output->gaps = next;
                }
                else {
                    prev->next = p->next;
                }
                output_free_gap(output, p);
            }
        }

        prev = p;

        p = next;
    }

    p = new_gaps;

    while(p != NULL) {
        next = p->next;
        output_add_gap(output, p);
        p = next;
    }

    return BOO_OK;
}

static boo_int_t
output_add_transition(boo_output_t *output, boo_uint_t state, boo_uint_t symbol, boo_int_t target)
{
    boo_output_transition_t *nt, *before, *after;

    nt = pcalloc(output->pool, sizeof(boo_output_transition_t));

    if(nt == NULL) {
        return BOO_ERROR;
    }

    nt->input = symbol;
    nt->target = target;

    if(output->states[state].transitions != NULL) {
        after = NULL;
        before = output->states[state].transitions;

        while(before != NULL) {
            if(before->input > nt->input) {
                break;
            }
            after = before;
            before = before->next;
        }

        if(after == NULL) {
            nt->next = before;
            output->states[state].transitions = nt;
        }
        else {
            nt->next = before;
            after->next = nt;
        }
    }
    else {
        nt->next = NULL;
        output->states[state].transitions = nt;
    }

    return BOO_OK;
}

/*
 * Returns BOO_OK if it is possible to fit specified transition table at specified base,
 * Returns BOO_DECLINED otherwise
 */
static boo_int_t
output_can_store_transitions(boo_output_t *output, boo_int_t base, boo_output_state_t *n)
{
    boo_output_transition_t *t;
    boo_uint_t from, to;
    boo_gap_t *g;

    t = n->transitions;
    g = output->gaps;

    while(t != NULL) {
        from = base + t->input;

        if(from >= output->end) {
            return BOO_OK;
        }

        to = from + 1;

        if(g == NULL) {
            fprintf(stderr, "fatal error: no more gaps, but there are more symbols\n");
            return BOO_DECLINED;
        }

        if((g->from < to) && (g->to > from)) {
            t = t->next;

            if(g->to == to) {
                g = g->next;
            }
        }
        else {
            if(g->from < from) {
                if(g->to >= from) {
                    return BOO_DECLINED;
                }
                g = g->next;
            }
            else {
                return BOO_DECLINED;
            }
        }
    }

    return BOO_OK;
}

static boo_int_t
output_book_space(boo_output_t *output, boo_int_t base, boo_output_state_t *n) {
    boo_output_transition_t *t;
    boo_uint_t pos;
    boo_gap_t *new_gap;
    boo_int_t rc;

    for(t = n->transitions ; t != NULL ; t = t->next) {
        pos = base + t->input;

        if(pos >= output->end) {
            if(pos != output->end) {
                new_gap = output_alloc_gap(output);

                if(new_gap == NULL) {
                    return BOO_ERROR;
                }

                new_gap->from = output->end;
                new_gap->to = pos;
                new_gap->next = NULL;

                output_add_gap(output, new_gap);
            }

            output->end = pos + 1;

            fprintf(output->debug, "extending space to %i, end is now %i\n", pos, output->end);
        }

        fprintf(output->debug, "placing symbol %u of state %d pointing to %d, pos %d\n", t->input, n - output->states, t->target, pos);

        rc = output_fill_gap(output, pos, pos + 1);

        if(rc != BOO_OK) {
            return rc;
        }
    }

    return BOO_OK;
}

static boo_int_t
output_index_states(boo_output_t *output) {
    boo_output_state_t *p, *q;
    boo_output_transition_t *t;
    boo_int_t base, offset;
    boo_int_t rc;
    boo_int_t *pi;

    p = output->states;
    q = p + output->num_states;

    while(p != q) {
        if(p->transitions == NULL) {
            p++;
            continue;
        }

        base = (output->gaps != NULL ? output->gaps->from : output->end)
            - p->transitions->input;
        
        for(offset = base ; offset != (boo_int_t)output->end ; offset += 1) {
            rc = output_can_store_transitions(output, offset, p);

            if(rc == BOO_ERROR) {
                return rc;
            }

            if(rc == BOO_OK) {
                break;
            }
        }

        p->base = offset;

        rc = output_book_space(output, offset, p);

        if(rc == BOO_ERROR) {
            return rc;
        }

        p++;
    }

    output->next = pcalloc(output->pool, output->end * boo_trans_len);

    if(output->next == NULL) {
        return BOO_ERROR;
    }

    p = output->states;

    while(p != q) {
        if(p->transitions == NULL) {
            p++;
            continue;
        }

        for(t = p->transitions ; t != NULL ; t = t->next) {
            pi = (boo_int_t*)&output->next[p->base * boo_trans_len + t->input * boo_trans_len];

            *pi = p - output->states; // State number

            pi++;

            *pi = t->target;
        }

        p++;
    }

    return BOO_OK;
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
    boo_uint_t i;

    fprintf(output->file, "static const short boo_base[] = {\n");

    for(i = 0 ; i != output->num_states ; i++) {
        fprintf(output->file, "%6d,", output->states[i].base);

        if(i % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n\n");

    return BOO_OK;
}

boo_int_t output_action(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t i, *pi;
    boo_uint_t n = 0;

    fprintf(output->file, "static const char boo_next[] = {\n");

    pi = (boo_int_t*)output->next;

    for(i = 0 ; i != output->end ; i++) {
        fprintf(output->file, "%6d,", pi[1]);

        if(n % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }
        n++; pi += 2;
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n\n");

    return BOO_OK;
}

boo_int_t output_check(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t i, *pi;
    boo_uint_t n = 0;

    fprintf(output->file, "static const char boo_check[] = {\n");

    pi = (boo_int_t*)output->next;

    for(i = 0 ; i != output->end ; i++) {
        fprintf(output->file, "%6d,", pi[0]);

        if(n % output->row_stride == output->row_stride - 1) {
            fprintf(output->file, "\n");
        }
        n++; pi += 2;
    }

    fprintf(output->file, "\n");

    fprintf(output->file, "};\n");

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
                fprintf(output->debug, "adding lookahead '%c' to state %d -> %d\n", symbol, state, next);
            }
            else {
                fprintf(output->debug, "adding lookahead %d to state %d -> %d\n", symbol, state, next);
            }
        }
        else {
            symbol = 0; next = 0;
            fprintf(output->debug, "adding lookahead $eof to state %d -> $accept\n", state);
        }

        rc = output_add_transition(output, state, symbol, -item->rule_n);
 
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
    boo_uint_t symbol;
    boo_lalr1_item_set_t *item_set;
    boo_lalr1_item_t *item;
    boo_lhs_lookup_t *lhs_lookup;

    output->states = pcalloc(output->pool, grammar->num_item_sets * sizeof(boo_output_state_t));

    if(output->states == NULL) {
        return BOO_ERROR;
    }

    output->num_states = grammar->num_item_sets;

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
                        fprintf(output->debug, "adding transition %d to state %d -> %d\n",
                            symbol, item_set->state_n,
                            item->transition->item_set->state_n);
                    }

                    rc = output_add_transition(output, item_set->state_n, symbol,
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
#if 0
                rc = darray_insert(output->darray, item_set->state_n,
                    boo_token_get(item->rhs[item->pos]), 0);

                if(rc != BOO_OK) {
                    return rc;
                }
#endif
            }
            
            item = boo_list_next(item);
        }

        item_set = boo_list_next(item_set);
    }

    return output_index_states(output);
}
