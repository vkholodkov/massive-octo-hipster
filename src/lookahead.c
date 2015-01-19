
#include <stdio.h>

#include "grammar.h"

#define multiple_reductions 0

static boo_int_t
lookahead_add_item(boo_grammar_t*, boo_lalr1_item_t*, boo_uint_t, boo_uint_t);

static void
lookahead_item_from_rule(boo_lalr1_item_t *item, boo_rule_t *rule)
{
    item->rule_n = rule->rule_n;
    item->lhs = rule->lhs;
    item->rhs = rule->rhs;
    item->length = rule->length;
    item->pos = item->length;
    item->transition = NULL;
}

static boo_int_t
lookahead_close_item_set(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set)
{
    boo_lalr1_item_t *item, *new_item;
    boo_rule_t *rule;
    boo_list_t add_queue;
    u_char seen_token = 0;
    u_char seen_nonterminal_in_core = 0;
    boo_uint_t i, num_nonterminals;

    boo_list_init(&add_queue);

    do{
        boo_list_splice(&item_set->items, &add_queue);

        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {
            if(item->core && item->pos != item->length && !boo_is_token(item->rhs[item->pos - 1])) {
                seen_nonterminal_in_core = 1;
            }

            if(!item->closed) {
                /*
                 * Find all items that have current position
                 * behind a non-terminal
                 */
                if(item->pos != 0 && !boo_is_token(item->rhs[item->pos - 1])) {
                    /*
                     * Lookup all rules that have a symbol in question
                     * on the left-hand side
                     */
                    rule = grammar_lookup_rules_by_lhs(grammar, item->rhs[item->pos - 1]);

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

                            lookahead_item_from_rule(new_item, rule);

                            if(rule->length != 0 && boo_is_token(rule->rhs[0])) {
                                seen_token = 1;
                            }

                            num_nonterminals = 0;

                            for(i = 0 ; i != new_item->length ; i++) {
                                if(boo_is_token(new_item->rhs[i])) {
                                    num_nonterminals++;
                                }
                            }

                            new_item->remove = item->remove + num_nonterminals;
                            new_item->original_symbol = item->core ? item->rhs[item->pos - 1]
                                : item->original_symbol;
                            new_item->instantiated_from = item;

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
    }

    return BOO_OK;
}

static boo_int_t
lookahead_close_item_sets(boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        rc = lookahead_close_item_set(grammar, item_set);

        if(rc == BOO_ERROR) {
            return rc;
        }

#if 0
        fprintf(grammar->debug, "Closed item set:\n");
        grammar_dump_item_set(grammar->debug, grammar, item_set);
#endif

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

static boo_int_t
lookahead_remove_duplicate_core_sets(boo_grammar_t *grammar, boo_list_t *item_sets, boo_list_t *result_sets)
{
    boo_lalr1_item_set_t *item_set1, *item_set2, *tmp;
    boo_transition_t *t, *p;

    item_set1 = boo_list_begin(item_sets);

    while(item_set1 != boo_list_end(item_sets)) {

        item_set2 = boo_list_begin(result_sets);

        while(item_set2 != boo_list_end(result_sets)) {
            if(grammar_core_sets_match(grammar, item_set1, item_set2)) {
                tmp = boo_list_next(item_set2);

#if 0
                printf("Removing duplicate core set:\n");
                grammar_dump_item_set(grammar, item_set2);
#endif

                boo_list_remove(&item_set2->entry);

                p = NULL;
                t = item_set2->transitions;

                while(t != NULL) {
                    t->item_set = item_set1;
                    p = t;
                    t = t->next;
                }

                if(p != NULL) {
                    p->next = item_set1->transitions;
                    item_set1->transitions = p;
                }

                grammar_free_item_set(grammar, item_set2);

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
lookahead_find_transitions(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set, boo_list_t *item_sets, boo_list_t *result_set)
{
    boo_uint_t symbol;
    boo_lalr1_item_t *item, *new_item;
    boo_lalr1_item_set_t *new_item_set;
    boo_trans_lookup_t *lookup;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {
        if(item->pos != 1 && boo_token_get(item->rhs[item->pos - 1]) != BOO_EOF)
        {
            symbol = boo_token_get(item->rhs[item->pos - 1]);
            lookup = grammar->transition_lookup + symbol;

            new_item_set = lookup->item_set;

            if(new_item_set == NULL) {
                new_item_set = grammar_alloc_item_set(grammar);

                if(new_item_set == NULL) {
                    return BOO_ERROR;
                }

                boo_list_init(&new_item_set->items);

                lookup->item_set = new_item_set;
                lookup->next = grammar->first_used_trans;
                grammar->first_used_trans = lookup;
            }

            new_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

            if(new_item == NULL) {
                return BOO_ERROR;
            }

            new_item->rule_n = item->rule_n;
            new_item->lhs = item->lhs;
            new_item->length = item->length;
            new_item->rhs = item->rhs;
            new_item->pos = item->pos - 1;
            new_item->core = 1;

            grammar_item_set_add_item(new_item_set, new_item);
        }

        item = boo_list_next(item);
    }

    lookup = grammar->first_used_trans;

    while(lookup != NULL) {
        boo_list_prepend(result_set, &lookup->item_set->entry);

        lookup->item_set = NULL;
        lookup->transition = NULL;

        lookup = lookup->next;
    }

    grammar->first_used_trans = NULL;

    return BOO_OK;
}

static boo_int_t
lookahead_find_transitions_for_set(boo_grammar_t *grammar, boo_list_t *item_sets, boo_list_t *result_set)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        rc = lookahead_find_transitions(grammar, item_set, item_sets, result_set);

        if(rc == BOO_ERROR) {
            return rc;
        }

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

static boo_int_t
lookahead_build_item_sets(boo_grammar_t *grammar, boo_list_t *dest)
{
    boo_int_t rc;
    boo_list_t to_process, add_queue;

    boo_list_init(&to_process);
    boo_list_init(&add_queue);

    rc = lookahead_close_item_sets(grammar, dest);

    if(rc == BOO_ERROR) {
        return BOO_ERROR;
    }
#if 0
    printf("closed item set\n");
    grammar_dump_item_sets(grammar, dest);
#endif
    rc = lookahead_find_transitions_for_set(grammar, dest, &add_queue);

    if(rc == BOO_ERROR) {
        return BOO_ERROR;
    }
#if 0
    printf("transitions\n");
    grammar_dump_item_sets(grammar, dest);
#endif
    do {
        boo_list_splice(&to_process, &add_queue);

        rc = lookahead_close_item_sets(grammar, &to_process);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }
#if 0
        printf("+closed item set\n");
        grammar_dump_item_sets(grammar, dest);
#endif
        rc = lookahead_find_transitions_for_set(grammar, &to_process, &add_queue);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }

        boo_list_splice(dest, &to_process);

        rc = lookahead_remove_duplicate_core_sets(grammar, dest, &add_queue);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }
    } while(!boo_list_empty(&add_queue));

    return BOO_OK;
}

boo_int_t lookahead_generate_item_sets(boo_grammar_t *grammar, boo_list_t *dest)
{
    boo_lalr1_item_set_t *root_item_set;
    boo_lalr1_item_t *root_item;
    boo_rule_t *root_rule;

    root_item_set = pcalloc(grammar->pool, sizeof(boo_lalr1_item_set_t));

    if(root_item_set == NULL) {
        return BOO_ERROR;
    }

    boo_list_init(&root_item_set->items);

    boo_list_append(dest, &root_item_set->entry);

    root_rule = grammar->lhs_lookup[boo_code_to_symbol(BOO_START)].rules;

    if(root_rule == NULL) {
        return BOO_ERROR;
    }

    while(root_rule != NULL) { 
        root_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

        if(root_item_set == NULL) {
            return BOO_ERROR;
        }

        lookahead_item_from_rule(root_item, root_rule);

        root_item->pos--;
        root_item->core = 1;

        boo_list_append(&root_item_set->items, &root_item->entry);

        root_rule = root_rule->lhs_hash_next;
    }

    return lookahead_build_item_sets(grammar, dest);
    
}

static boo_int_t
lookahead_add_first_set(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set, boo_lalr1_item_t *dest)
{
    boo_int_t rc;
    boo_lalr1_item_t *item;
#if 0
    grammar_dump_item(grammar, dest);
#endif
    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {
        if(item->pos != item->length && boo_is_token(item->rhs[item->pos]) && 
            item->transition != NULL)
        {
            rc = lookahead_add_item(grammar, dest,
                boo_token_get(item->rhs[item->pos]),
                item->transition->item_set->state_n);

            if(rc != BOO_OK) {
                return rc;
            }
        }

        item = boo_list_next(item);
    }

    return BOO_OK;
}

static boo_int_t
lookahead_find_first_set(boo_grammar_t *grammar, boo_lalr1_item_t *v, boo_lalr1_item_t *dest)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;
    boo_lalr1_item_t *item;
    boo_uint_t matches_core_item;

    item_set = boo_list_begin(&grammar->item_sets);

    while(item_set != boo_list_end(&grammar->item_sets)) {

        matches_core_item = 0;

        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {
            if(item->core && item->lhs == v->lhs && item->length == v->length && 
                item->pos == v->pos && !memcmp(item->rhs, v->rhs, item->length * sizeof(boo_uint_t)))
            {
                matches_core_item = 1;
            }

            item = boo_list_next(item);
        }

        if(matches_core_item) {
            rc = lookahead_add_first_set(grammar, item_set, dest);

            if(rc != BOO_OK) {
                return rc;
            }
        }

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

static boo_int_t
lookahead_lookup_state(boo_grammar_t *grammar, boo_lalr1_item_t *item, boo_uint_t *state)
{
    boo_trie_t *t = grammar->core_set_index;
    boo_trie_node_t *n;
    boo_uint_t *p, *q, symbol, *pstate;

    if(state == NULL) {
        return BOO_DECLINED;
    }

    n = boo_trie_next(t, t->root, item->lhs);

    if(n == NULL) {
        return BOO_DECLINED;
    }

    p = item->rhs;
    q = p + item->length;

    while(p != q) {
        symbol = boo_token_get(*p);

        n = boo_trie_next(t, n, symbol);

        if(n == NULL) {
            return BOO_DECLINED;
        }

        p++;
    }

    n = boo_trie_next(t, n, item->pos + 1);

    if(n == NULL || state == NULL) {
        return BOO_DECLINED;
    }

    pstate = n->leaf;

    *state = *pstate;

    return BOO_OK;
}

static boo_int_t
lookahead_add_item(boo_grammar_t *grammar, boo_lalr1_item_t *item, boo_uint_t sym, boo_uint_t state)
{
    boo_trie_t *t = grammar->lookahead_set;
    boo_trie_node_t *n;
    boo_uint_t *p, *q, symbol;
#if multiple_reductions
    boo_uint_t *actions, num_actions;
    boo_lalr1_item_t *i;
#endif
    boo_reduction_t *reduction;
#if multiple_reductions
    boo_uint_t conflict;
#endif

    /*
     * Add an element corresponding to the left-hand-side
     */
    n = boo_trie_add_sequence(t, t->root, &item->lhs, &item->lhs + 1);

    if(n == NULL) {
        return BOO_ERROR;
    }

    /*
     * Add all symbols of the right-hand-side
     */
    p = item->rhs;
    q = p + item->length;

    while(p != q) {
        symbol = boo_token_get(*p);

        n = boo_trie_add_sequence(t, n, &symbol, &symbol + 1);

        if(n == NULL) {
            return BOO_ERROR;
        }

        p++;
    }

    /*
     * Add an element corresponding to the lookahead symbol
     */
    n = boo_trie_add_sequence(t, n, &sym, &sym + 1);

    if(n == NULL) {
        return BOO_ERROR;
    }

#if multiple_reductions
    i = item;

    num_actions = 0;

    while(i != NULL && !i->core) {
        num_actions++;
        i = i->instantiated_from;
    }
#endif

    /*
     * We've reached a leaf of the trie
     */
    if(n->leaf != NULL) {
        reduction = n->leaf;
#if multiple_reductions
        conflict = 0;

        if(reduction->num_actions == num_actions) {
            i = item; actions = reduction->actions;

            while(i != NULL && !i->core) {
                if(*actions != i->rule_n) {
                    conflict = 1;
                    break;
                }
                i = i->instantiated_from; actions++;
            }
        }
        else {
            conflict = 1;
        }
        if(conflict) {
#else
        if(reduction->rule_n != item->rule_n) {
#endif
            fprintf(stderr, "Multiple definitions of rule in the grammar:\n");
            grammar_dump_rule_from_item(stderr, grammar, item);
        }
        return BOO_OK;
    }

    reduction = pcalloc(grammar->pool, sizeof(boo_reduction_t));

    if(reduction == NULL) {
        return BOO_ERROR;
    }

    reduction->rule_n = item->rule_n;

#if multiple_reductions
    reduction->num_actions = num_actions;

    reduction->actions = actions = pcalloc(grammar->pool, reduction->num_actions * sizeof(boo_uint_t));

    if(actions == NULL) {
        return BOO_ERROR;
    }

    /*
     * Build reduction list
     */
    i = item;

    while(i != NULL && !i->core) {
        *actions++ = i->rule_n;
        i = i->instantiated_from;
    }
#endif

    n->leaf = reduction;

    boo_list_append(&grammar->reductions, &reduction->entry);

    return BOO_OK;
}

static boo_int_t
lookahead_add_item_set(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set)
{
    boo_int_t rc;
    boo_lalr1_item_t *item1, *item2;
    boo_uint_t symbol;
    boo_uint_t state;

    item1 = boo_list_begin(&item_set->items);
    
    while(item1 != boo_list_end(&item_set->items)) {
        if(item1->pos != item1->length) {
            /*
             * Check if the symbol next to the current position is a token
             */
            if(boo_is_token(item1->rhs[item1->pos])) {
                symbol = boo_token_get(item1->rhs[item1->pos]);

                /*
                 * Lookup this item in the first set
                 */
                if(symbol != BOO_EOF) {
                    rc = lookahead_lookup_state(grammar, item1, &state);

                    if(rc != BOO_OK) {
                        fprintf(stdout, "Cannot resolve state of item ");
                        grammar_dump_item(stdout, grammar, item1);
                        return BOO_ERROR;
                    }
                }
                else {
                    state = 0;
                }

                item2 = boo_list_begin(&item_set->items);

                /*
                 * Scan over items of the item set and add those
                 * that have the marker at the end to the dictionary
                 * and those left-hand-side matches the symbol in the front of the marker
                 * of the item above
                 */
                while(item2 != boo_list_end(&item_set->items)) {
                    if(item2->pos == item2->length && item2->original_symbol == item1->rhs[item1->pos - 1]) {
                        rc = lookahead_add_item(grammar, item2, symbol, state);

                        if(rc == BOO_ERROR) {
                            return rc;
                        }
                    }
 
                    item2 = boo_list_next(item2);
                }
            }
            else {
#if 0
                printf("Add first set of item:\n");
                grammar_dump_item(grammar, item1);

                printf("To items:\n");
#endif
                item2 = boo_list_begin(&item_set->items);

                while(item2 != boo_list_end(&item_set->items)) {
                    if(item2->pos == item2->length) {

                        rc = lookahead_find_first_set(grammar, item1, item2);

                        if(rc == BOO_ERROR) {
                            return rc;
                        }
                    }

                    item2 = boo_list_next(item2);
                }
            }
        }

        item1 = boo_list_next(item1);
    }

    return BOO_OK;
}

boo_int_t lookahead_build(boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;

    grammar->lookahead_set = tree_create(grammar->pool);

    if(grammar->lookahead_set == NULL) {
        return BOO_ERROR;
    }

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        rc = lookahead_add_item_set(grammar, item_set);

        if(rc == BOO_ERROR) {
            return rc;
        }

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

