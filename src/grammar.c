
#include <stdio.h>

#include "grammar.h"

static const boo_str_t accept_symbol_name = boo_string("$accept");

static void grammar_dump_item(boo_grammar_t*, boo_lalr1_item_t*);
static void grammar_dump_item_set(boo_grammar_t*, boo_lalr1_item_set_t*);

boo_grammar_t *grammar_create(pool_t *p)
{
    boo_grammar_t *grammar;

    grammar = pcalloc(p, sizeof(boo_grammar_t));

    if(grammar == NULL) {
        return NULL;
    }

    grammar->pool = p;

    boo_list_init(&grammar->rules);
    boo_list_init(&grammar->actions);
    boo_list_init(&grammar->item_sets);

    grammar->symtab = symtab_create(p);

    if(grammar->symtab == NULL) {
        return NULL;
    }

    return grammar;
}

boo_int_t grammar_wrapup(boo_grammar_t *grammar) {
    boo_int_t i;
    boo_lhs_lookup_t *lhs_lookup;
    symbol_t *symbol;

    grammar->transition_lookup = pcalloc(grammar->pool,
        (grammar->num_symbols + UCHAR_MAX + 1) * sizeof(boo_lalr1_item_set_t*));    

    if(grammar->transition_lookup == NULL) {
        return BOO_ERROR;
    }

    lhs_lookup = grammar->lhs_lookup;

    for(i = 0 ; i != grammar->num_symbols + 1 ; i++) {
        if(lhs_lookup[i].rules == NULL) {
            symbol = symtab_resolve(grammar->symtab, &grammar->lhs_lookup[i].name);

            if(symbol != NULL && !boo_is_token(symbol->value)) {
                fprintf(stderr, "non-terminal %s is used in right-hand side but has no rules\n",
                    grammar->lhs_lookup[i].name.data);
                return BOO_ERROR;
            }
        }
    }

    return BOO_OK;
}

void grammar_add_rule(boo_grammar_t *grammar, boo_rule_t *rule)
{
    boo_int_t i;

    boo_list_append(&grammar->rules, &rule->entry);

    printf("%x -> ", rule->lhs);

    for(i = 0 ; i != rule->length ; i++) {
        printf("%x ", rule->rhs[i]);
    }

    printf("\n");
}

static void
grammar_item_from_rule(boo_lalr1_item_t *item, boo_rule_t *rule)
{
    item->lhs = rule->lhs;
    item->rhs = rule->rhs;
    item->length = rule->length;
    item->pos = 0;
}

static boo_int_t
grammar_close_item_set(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set)
{
    boo_lalr1_item_t *item, *new_item;
    boo_rule_t *rule;
    boo_list_t add_queue;
    u_char seen_token = 0;
    u_char seen_nonterminal_in_core = 0;

    boo_list_init(&add_queue);

    do{
        boo_list_splice(&item_set->items, &add_queue);

        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {
            if(item->core && item->pos != item->length && !boo_is_token(item->rhs[item->pos])) {
                seen_nonterminal_in_core = 1;
            }

            if(!item->closed) {
                /*
                 * Find all items that have current position
                 * in front of a non-terminal
                 */
                if(item->pos != item->length && !boo_is_token(item->rhs[item->pos])) {
                    /*
                     * Lookup all rules that have a symbol in question
                     * on the left-hand side
                     */
                    rule = grammar_lookup_rules_by_lhs(grammar, item->rhs[item->pos]);

                    while(rule != NULL) {
                        /*
                         * Skip if we already used this rule in this item set
                         */
                        if(rule->booked_by_item_set != item_set) {
                            // Convert the rule to an item 
                            new_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

                            if(new_item == NULL) {
                                return BOO_ERROR;
                            }

                            grammar_item_from_rule(new_item, rule);

                            if(rule->length != 0 && boo_is_token(rule->rhs[0])) {
                                seen_token = 1;
                            }

                            /*
                             * Book this rule (booking is only valid for the current item set)
                             */
                            rule->booked_by_item_set = item_set;

                            boo_list_append(&add_queue, &new_item->entry);
                        }

                        rule = rule->lhs_hash_next;
                    }
                }

                item->closed = 1;
            }

            item = boo_list_next(item);
        }
        /*
         * Do until there is nothing more to close
         */
    } while(!boo_list_empty(&add_queue));

    if(seen_nonterminal_in_core && !seen_token) {
        fprintf(stderr, "cannot close an item set: grammar is incomplete\n");
        return BOO_ERROR;
    }

    return BOO_OK;
}

static boo_int_t
grammar_close_item_sets(boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        rc = grammar_close_item_set(grammar, item_set);

        if(rc == BOO_ERROR) {
            return rc;
        }

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

static boo_int_t
grammar_core_sets_match(boo_lalr1_item_set_t *item_set1, boo_lalr1_item_set_t *item_set2)
{
    boo_lalr1_item_t *item1;
    boo_lalr1_item_t *item2;
    boo_uint_t num_core1 = 0;
    boo_uint_t num_core2 = 0;

    item1 = boo_list_begin(&item_set1->items);

    while(item1 != boo_list_end(&item_set1->items)) {

        if(item1->core) {
            num_core1++;
        }

        item2 = boo_list_begin(&item_set2->items);

        while(item2 != boo_list_end(&item_set2->items)) {
            if(item1->core && item2->core && item1->lhs == item2->lhs && item1->length == item2->length && 
                item1->pos == item2->pos && !memcmp(item1->rhs, item2->rhs, item1->length * sizeof(boo_uint_t)))
            {
                num_core2++;
            }

            item2 = boo_list_next(item2);
        }

        item1 = boo_list_next(item1);
    }

    return (num_core1 == num_core2);
}

static boo_int_t
grammar_remove_duplicate_core_sets(boo_grammar_t *grammar, boo_list_t *item_sets, boo_list_t *result_sets)
{
    boo_lalr1_item_set_t *item_set1, *item_set2, *tmp;

    item_set1 = boo_list_begin(item_sets);

    while(item_set1 != boo_list_end(item_sets)) {

        item_set2 = boo_list_begin(result_sets);

        while(item_set2 != boo_list_end(result_sets)) {
            if(grammar_core_sets_match(item_set1, item_set2)) {
                tmp = boo_list_next(item_set2);

#if 0
                printf("Removing duplicate core set:\n");
                grammar_dump_item_set(grammar, item_set2);
#endif

                boo_list_remove(&item_set2->entry);

                grammar->num_item_sets--;

                item_set2 = tmp;
            }
            else {
                item_set2 = boo_list_next(item_set2);
            }
        }

        item_set1 = boo_list_next(item_set1);
    }

    return BOO_OK;
}

static boo_int_t
grammar_find_transitions(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set, boo_list_t *item_sets, boo_list_t *result_set)
{
    boo_uint_t symbol;
    boo_lalr1_item_t *item, *new_item;
    boo_lalr1_item_set_t *new_item_set;
    void **lookup;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {
        if(item->pos != item->length && boo_token_get(item->rhs[item->pos]) != BOO_ACCEPT)
        {
            symbol = boo_token_get(item->rhs[item->pos]);
            lookup = grammar->transition_lookup + symbol;

            new_item_set = *lookup;

            if(new_item_set == NULL) {
                new_item_set = pcalloc(grammar->pool, sizeof(boo_lalr1_item_set_t));

                if(new_item_set == NULL) {
                    return BOO_ERROR;
                }

                boo_list_init(&new_item_set->items);

                *lookup = new_item_set;

                boo_list_append(result_set, &new_item_set->entry);

                grammar->num_item_sets++;
            }

            new_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

            if(new_item == NULL) {
                return BOO_ERROR;
            }

            new_item->lhs = item->lhs;
            new_item->length = item->length;
            new_item->rhs = item->rhs;
            new_item->pos = item->pos + 1;
            new_item->core = 1;

            boo_list_append(&new_item_set->items, &new_item->entry);

            if(new_item->pos == new_item->length) {
                new_item_set->has_reductions = 1;
            }
        }

        item = boo_list_next(item);
    }

    memset(grammar->transition_lookup, 0, boo_symbol_to_code(grammar->num_symbols) * sizeof(void*));

    return BOO_OK;
}

static boo_int_t
grammar_find_transitions_for_set(boo_grammar_t *grammar, boo_list_t *item_sets, boo_list_t *result_set)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        rc = grammar_find_transitions(grammar, item_set, item_sets, result_set);

        if(rc == BOO_ERROR) {
            return rc;
        }

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

static boo_int_t
grammar_build_item_sets(boo_grammar_t *grammar)
{
    boo_int_t rc;
    boo_list_t to_process, add_queue;

    boo_list_init(&to_process);
    boo_list_init(&add_queue);

    rc = grammar_close_item_sets(grammar, &grammar->item_sets);

    if(rc == BOO_ERROR) {
        return BOO_ERROR;
    }

    rc = grammar_find_transitions_for_set(grammar, &grammar->item_sets, &add_queue);

    if(rc == BOO_ERROR) {
        return BOO_ERROR;
    }

    do {
        boo_list_splice(&to_process, &add_queue);

        rc = grammar_close_item_sets(grammar, &to_process);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }

        rc = grammar_find_transitions_for_set(grammar, &to_process, &add_queue);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }

        boo_list_splice(&grammar->item_sets, &to_process);

        rc = grammar_remove_duplicate_core_sets(grammar, &grammar->item_sets, &add_queue);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }
    } while(!boo_list_empty(&add_queue));

    return BOO_OK;
}

boo_int_t grammar_generate_lr_item_sets(boo_grammar_t *grammar)
{
    boo_lalr1_item_set_t *root_item_set;
    boo_lalr1_item_t *root_item;
    boo_rule_t *root_rule;

    root_rule = boo_list_begin(&grammar->rules);

    if(root_rule == boo_list_end(&grammar->rules)) {
        return BOO_ERROR;
    }

    root_item_set = pcalloc(grammar->pool, sizeof(boo_lalr1_item_set_t));

    if(root_item_set == NULL) {
        return BOO_ERROR;
    }

    boo_list_init(&root_item_set->items);

    boo_list_append(&grammar->item_sets, &root_item_set->entry);

    grammar->num_item_sets++;

    root_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

    if(root_item_set == NULL) {
        return BOO_ERROR;
    }

    grammar_item_from_rule(root_item, root_rule);

    root_item->core = 1;

    boo_list_append(&root_item_set->items, &root_item->entry);

    return grammar_build_item_sets(grammar);
}

boo_int_t grammar_generate_lookahead_sets(boo_grammar_t *grammar)
{
    return BOO_OK;
}

static void
grammar_dump_item(boo_grammar_t *grammar, boo_lalr1_item_t *item) {
    boo_int_t i;

    printf(item->core ? "%s -> " : "+%s -> ", grammar->lhs_lookup[boo_code_to_symbol(item->lhs)].name.data);

    for(i = 0 ; i != item->length ; i++) {
        if(item->pos == i) {
            printf(". ");
        }

        if(boo_is_token(item->rhs[i]) && boo_token_get(item->rhs[i]) <= UCHAR_MAX) {
            printf("\'%c\' ", boo_token_get(item->rhs[i]));
        }
        else {
            if(boo_token_get(item->rhs[i]) == BOO_ACCEPT) {
                printf("$accept ");
            }
            else {
                printf("%s ", grammar->lhs_lookup[boo_code_to_symbol(item->rhs[i])].name.data);
            }
        }
    }

    if(item->pos == i) {
        printf(". ");
    }

    printf("\n");
}

static void
grammar_dump_item_set(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set)
{
    boo_lalr1_item_t *item;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {

        grammar_dump_item(grammar, item);

        item = boo_list_next(item);
    }
}

void grammar_dump_item_sets(boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_uint_t item_set_n = 0;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        printf("Item set %d:\n", item_set_n);

        grammar_dump_item_set(grammar, item_set);

        printf("\n");

        item_set = boo_list_next(item_set);
        item_set_n++;
    }
}
