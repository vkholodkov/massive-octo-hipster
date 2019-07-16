
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

    fprintf(output->file, "#define ");
    boo_puts_upper(output->file, grammar->prefix);
    fprintf(output->file, "_EOF %d\n", UCHAR_MAX + 1);

    for(i = 0 ; i != grammar->num_symbols ; i++) {
        if(grammar->lhs_lookup[i].token && !grammar->lhs_lookup[i].literal) {
            fprintf(output->file, "#define ");
            boo_puts_upper(output->file, grammar->prefix);
            fprintf(output->file, "_");
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

    fprintf(output->file, "static const char *");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_symbol_table[] = {\n      ");

    fprintf(output->file, "\"$eof\",\n      ");

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

    fprintf(output->file, "static const char **");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_symbol = &");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_symbol_table[1];\n\n");

    return BOO_OK;
}

boo_int_t output_lookup(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t rc;
    boo_str_t prefix;
    u_char *p;

    prefix.len = grammar->prefix->len + sizeof("_t") - 1;
    prefix.data = palloc(output->pool, prefix.len);

    if(prefix.data == NULL) {
        return BOO_ERROR;
    }

    p = boo_strcpy(prefix.data, grammar->prefix);

    *p++ = '_';
    *p++ = 't';

    rc = lookup_write(output->file, output->term, &prefix);

    if(rc != BOO_OK) {
        return rc;
    }

    prefix.len = grammar->prefix->len + sizeof("_nt") - 1;
    prefix.data = palloc(output->pool, prefix.len);

    if(prefix.data == NULL) {
        return BOO_ERROR;
    }

    p = boo_strcpy(prefix.data, grammar->prefix);

    *p++ = '_';
    *p++ = 'n';
    *p++ = 't';

    rc = lookup_write(output->file, output->nterm, &prefix);

    if(rc != BOO_OK) {
        return rc;
    }

    prefix.len = grammar->prefix->len + sizeof("_a") - 1;
    prefix.data = palloc(output->pool, prefix.len);

    if(prefix.data == NULL) {
        return BOO_ERROR;
    }

    p = boo_strcpy(prefix.data, grammar->prefix);

    *p++ = '_';
    *p++ = 'a';

    return lookup_write(output->file, output->actions, &prefix);
}

boo_int_t output_actions(boo_output_t *output, boo_grammar_t *grammar, const char *filename)
{
    boo_action_t *action;
    boo_rule_t *rule;
    FILE *fin;
    int c;
    unsigned ref;
    boo_uint_t pos, escape;

    fin = fopen(filename, "r");

    if(fin == NULL) {
        return BOO_ERROR;
    }


    if(grammar->union_code != NULL) {
        fprintf(output->file,
            "\ntypedef struct {\n" \
            "    unsigned int state;\n" \
            "    union ");

        fseek(fin, grammar->union_code->start, SEEK_SET);

        pos = grammar->union_code->start;

        while(pos != grammar->union_code->end) {
            c = fgetc(fin);

            if(c == EOF) {
                break;
            }

            fputc(c, output->file);

            pos++;
        }

        fprintf(output->file,
            " val;\n" \
            "} "
        );
        boo_puts_lower(output->file, grammar->prefix);
        fprintf(output->file, "_stack_elm_t;\n\n");
    }
    else {
        fprintf(output->file,
            "\ntypedef struct {\n" \
            "    unsigned int state;\n" \
            "    void *val;\n" \
            "} "
        );
        boo_puts_lower(output->file, grammar->prefix);
        fprintf(output->file, "_stack_elm_t;\n\n");
    }

    fprintf(output->file, "inline void ");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_action(unsigned int action, ");
    boo_puts(output->file, grammar->context);
    fprintf(output->file, " *context, ");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_stack_elm_t *top) {\n");
    fprintf(output->file, "    ");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_stack_elm_t next = *top;\n");
    fprintf(output->file, "    switch(action) {\n");

    action = boo_list_begin(&grammar->actions);

    while(action != boo_list_end(&grammar->actions)) {

        rule = action->rule;

        fprintf(output->file, "         case %d: ", action->action_n);

        fseek(fin, action->start, SEEK_SET);

        escape = 0; ref = 0;
        pos = action->start;

        while(pos != action->end) {
            c = fgetc(fin);

            if(c == EOF) {
                break;
            }

            if(escape) {
                if(c == '$') {
                    if(grammar->lhs_lookup[boo_code_to_symbol(rule->lhs)].type == NULL) {
                        fprintf(stderr, "symbol ");
                        boo_puts(stderr, &grammar->lhs_lookup[boo_code_to_symbol(rule->lhs)].name);
                        fprintf(stderr, " is used in an action but has no type assigned\n");
                        return BOO_ERROR;
                    }

                    fprintf(output->file, "top[%d].val.", 1 - action->rule->length);
                    boo_puts(output->file, &grammar->lhs_lookup[boo_code_to_symbol(rule->lhs)].type->name);
                    escape = 0;
                }
                else if(c >= '0' && c <= '9') {
                    ref = ref * 10 + (c - '0');
                }
                else {
                    if(ref != 0) {

                        if(ref > rule->length) {
                            fprintf(stderr, "A reference points to the item %u while the rule has only %u items\n", ref, rule->length);
                            return BOO_ERROR;
                        }

                        if(grammar->lhs_lookup[boo_code_to_symbol(rule->rhs[ref - 1])].type == NULL) {
                            fprintf(stderr, "Symbol ");
                            boo_puts(stderr, &grammar->lhs_lookup[boo_code_to_symbol(rule->rhs[ref - 1])].name);
                            fprintf(stderr, " is used in an action but has no type assigned\n");
                            return BOO_ERROR;
                        }

                        fprintf(output->file, "top[%d].val.", 1 - action->rule->length + ref - 1);
                        boo_puts(output->file, &grammar->lhs_lookup[boo_code_to_symbol(rule->rhs[ref - 1])].type->name);

                        ref = 0;

                        fputc(c, output->file);
                    }
                    else {
                        fputc('$', output->file); fputc(c, output->file);
                    }

                    escape = 0;
                }
            }
            else {
                if(c == '$') {
                    escape = 1;
                }
                else {
                    fputc(c, output->file);
                }
            }

            pos++;
        }

        fprintf(output->file, "\n             break;\n");

        action = boo_list_next(action);
    }

    fprintf(output->file, "    }\n};\n\n");

    fclose(fin);

    return BOO_OK;
}

boo_int_t output_lhs(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_uint_t n = 0;
    boo_rule_t *rule;

    fprintf(output->file, "static const unsigned int ");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_lhs[] = {\n");

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

    fprintf(output->file, "static const int ");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_rhs[] = {\n");

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

    fprintf(output->file, "static const unsigned int ");
    boo_puts_lower(output->file, grammar->prefix);
    fprintf(output->file, "_rule[] = {\n");

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
output_conflict(boo_output_t *output, boo_grammar_t *grammar, boo_lalr1_item_t *item,
    boo_uint_t state, boo_uint_t code, boo_int_t target1, boo_int_t target2)
{
    const char *conflict1_type;
    const char *conflict2_type;
    boo_lhs_lookup_t *lhs_lookup;
    boo_int_t tmp;
    boo_rule_t *rule;

    if(target2 >= 0) {
        tmp = target2;
        target2 = target1;
        target1 = tmp;
    }

    conflict1_type = target1 >= 0 ? "shift" : "reduce";
    conflict2_type = target2 >= 0 ? "shift" : "reduce";

    if(code != BOO_EOF) {
        lhs_lookup = grammar->lhs_lookup + boo_code_to_symbol(code);

        if(lhs_lookup->literal) {
            fprintf(stderr, "%s-%s conflict in state %u on '%c':\n", conflict1_type, conflict2_type,
                state, lhs_lookup->name.data[0]);
        }
        else {
            fprintf(stderr, "%s-%s conflict in state %u on ", conflict1_type, conflict2_type, state);
            boo_puts(stderr, &lhs_lookup->name);
            fprintf(stderr, ":\n");
        }
    }
    else {
        fprintf(stderr, "%s-%s conflict in state %u on $eof:\n", conflict1_type, conflict2_type, state);
    }

    grammar_dump_item(stderr, grammar, item);

    fprintf(stderr, "%s %d/%s ", conflict1_type, target1, conflict2_type);

    rule = boo_list_begin(&grammar->rules);

    while(rule != boo_list_end(&grammar->rules)) {
        if(rule->rule_n == -target2) {
            grammar_dump_rule(stderr, grammar, rule);
            break;
        }
        rule = boo_list_next(rule);
    }

    fprintf(stderr, "\n\n");
}

static boo_int_t
output_add_action(boo_output_t *output, boo_action_t *action, boo_uint_t state, boo_uint_t symbol)
{
    boo_int_t rc;

    /*
     * Check if the action to be added is conflicting
     * with an existing action and if so,
     * report an error
     */
    rc = lookup_get_transition(output->actions, state, symbol);

    if(rc != BOO_DECLINED && rc != action->action_n) {
        fprintf(output->debug, "unable to add action %d to state %d\n", action->action_n, state);
        return BOO_ERROR;
    }

    rc = lookup_add_transition(output->actions, state, symbol, action->action_n);

    if(rc != BOO_OK) {
        return rc;
    }

    return BOO_OK;
}

static boo_int_t
output_add_actions(boo_output_t *output, boo_uint_t state, boo_uint_t symbol, boo_lalr1_item_set_t *item_set)
{
    boo_int_t rc;
    boo_lalr1_item_t *item;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {

        if(item->pos != item->length && item->pos < item->num_actions && item->actions[item->pos] != NULL) {
            rc = output_add_action(output, item->actions[item->pos], state, symbol);

            if(rc != BOO_OK) {
                return rc;
            }
        }
        
        item = boo_list_next(item);
    }

    return BOO_OK;
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

    /*
     * Look up the item in the lookahead set
     */
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

    /*
     * Iterate over all transitions from this node
     */
    tr = n->from;

    while(tr != NULL) {
        if(tr->to->leaf != NULL) {
            reduction = tr->to->leaf;

            if(tr->input != BOO_EOF) {
                symbol = boo_code_to_symbol(tr->input);
                lhs_lookup = grammar->lhs_lookup + symbol;

                if(lhs_lookup->literal) {
                    symbol = lhs_lookup->name.data[0];
                    fprintf(output->debug, "adding lookahead '%c' to state %d reduce %d\n",
                        symbol, state, reduction->rule_n);
                }
                else {
                    symbol = lhs_lookup->code;
                    fprintf(output->debug, "adding lookahead ");
                    boo_puts(output->debug, &lhs_lookup->name);
                    fprintf(output->debug, " (%d) to state %d reduce %d\n", symbol,
                        state, reduction->rule_n);
                }
            }
            else {
                symbol = UCHAR_MAX + 1;
                fprintf(output->debug, "adding lookahead $eof to state %d accept\n", state);
            }

            /*
             * Check if the transition to be added is conflicting
             * with an existing transition and if so,
             * output the conflict
             */
            rc = lookup_get_transition(output->term, state, symbol);

            if(rc != BOO_DECLINED && rc != -reduction->rule_n) {
                output_conflict(output, grammar, item, state, tr->input, -reduction->rule_n, rc);
            }
            else {
                rc = lookup_add_transition(output->term, state, symbol, -reduction->rule_n);

                if(rc != BOO_OK) {
                    return rc;
                }
            }

            /*
             * Add an action
             */
            if(item->pos < item->num_actions && item->actions[item->pos] != NULL) {

                rc = output_add_action(output, item->actions[item->pos], state, symbol);

                if(rc != BOO_OK) {
                    return rc;
                }
            }
        }

        tr = tr->next;
    }

    return BOO_OK;
}

static boo_int_t
output_add_item(boo_output_t *output, boo_grammar_t *grammar,
  boo_uint_t state, boo_lalr1_item_t *item)
{
    boo_int_t rc;
    boo_uint_t symbol;
    boo_lhs_lookup_t *lhs_lookup;

    if(item->transition != NULL)
    {
        if(boo_is_token(item->rhs[item->pos])) {

            /*
             * The marker is in front of a terminal,
             * add an entry into the terminal lookup table
             */
            symbol = boo_code_to_symbol(item->rhs[item->pos]);
            lhs_lookup = grammar->lhs_lookup + symbol;

            if(lhs_lookup->literal) {
                symbol = lhs_lookup->name.data[0];

                fprintf(output->debug, "adding transition '%c' to state %d -> %d\n",
                    symbol, state,
                    item->transition->item_set->state_n);
            }
            else {
                symbol = lhs_lookup->code;

                fprintf(output->debug, "adding transition ");
                boo_puts(output->debug, &lhs_lookup->name);
                fprintf(output->debug, " (%d) to state %d -> %d\n",
                    symbol, state,
                    item->transition->item_set->state_n);
            }

            /*
             * Check if the transition to be added is conflicting
             * with an existing transition and if so,
             * output the conflict
             */
            rc = lookup_get_transition(output->term, state, symbol);

            if(rc != BOO_DECLINED && rc != item->transition->item_set->state_n) {
                output_conflict(output, grammar, item, state, item->rhs[item->pos],
                    item->transition->item_set->state_n, rc);
            }
            else {
                rc = lookup_add_transition(output->term, state, symbol,
                    item->transition->item_set->state_n);

                if(rc != BOO_OK) {
                    return rc;
                }
            }

            /*
             * Add actions
             */
            rc = output_add_actions(output, state, symbol, item->transition->item_set);

            if(rc != BOO_OK) {
                return rc;
            }
        }
        else /*if(item->core)*/ {

            /*
             * The marker is in front of a non-terminal,
             * add an item into non-terminal lookup table
             */
            rc = lookup_add_transition(output->nterm, state,
                boo_code_to_symbol(item->rhs[item->pos]),
                item->transition->item_set->state_n);

            if(rc != BOO_OK) {
                return rc;
            }
        }
    }
    else if(item->pos == item->length) {

        /*
         * The marker is at the end of a rule,
         * add lookahead transitions
         */
        rc = output_add_lookahead(output, grammar, state, item);

        if(rc != BOO_OK) {
            return rc;
        }
    }
    else if(boo_token_get(item->rhs[item->pos]) == BOO_EOF) {

        /*
         * The marker is in front of an $eof symbol,
         * add an entry into the terminal lookup table
         */
        rc = lookup_add_transition(output->term, state,
            256, 0);

        if(rc != BOO_OK) {
            return rc;
        }
    }

    return BOO_OK;
}

static boo_int_t
output_add_item_set(boo_output_t *output, boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set)
{
    boo_int_t rc;
    boo_lalr1_item_t *item;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {

        rc = output_add_item(output, grammar, item_set->state_n, item);

        if(rc != BOO_OK) {
            return rc;
        }
        
        item = boo_list_next(item);
    }

    return BOO_OK;
}

boo_int_t output_add_grammar(boo_output_t *output, boo_grammar_t *grammar)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;
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

    output->actions = lookup_create(output->pool, grammar->num_item_sets);

    if(output->actions == NULL) {
        return BOO_ERROR;
    }

    output->actions->debug = output->debug;

    item_set = boo_list_begin(&grammar->item_sets);

    while(item_set != boo_list_end(&grammar->item_sets)) {

        rc = output_add_item_set(output, grammar, item_set);

        if(rc != BOO_OK) {
            return rc;
        }

        item_set = boo_list_next(item_set);
    }

    rc = lookup_index(output->term);

    if(rc != BOO_OK) {
        return rc;
    }

    rc = lookup_index(output->nterm);

    if(rc != BOO_OK) {
        return rc;
    }

    return lookup_index(output->actions);
}
