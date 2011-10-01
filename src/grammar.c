
#include <stdio.h>

#include "grammar.h"

static void grammar_dump_item(boo_lalr1_item_t *item);
static void grammar_dump_item_set(boo_lalr1_item_set_t *item_set);

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

    boo_list_init(&add_queue);

    do{
        boo_list_splice(&item_set->items, &add_queue);

        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {
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

        printf("Closed item set:\n");

        grammar_dump_item_set(item_set);

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

/*
 * See if advancing position of a given item will lead to an existing item
 * TODO: rewrite to something faster
 */
static boo_int_t
grammar_is_in_core_items(boo_list_t *item_sets, boo_lalr1_item_t *v, boo_uint_t pos)
{
    boo_lalr1_item_t *item;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {
        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {
            if(item->core && item->lhs == v->lhs && item->length == v->length && 
                item->pos == pos && !memcmp(item->rhs, v->rhs, item->length * sizeof(boo_uint_t)))
            {
                return 1;
            }

            item = boo_list_next(item);
        }

        item_set = boo_list_next(item_set);
    }

    return 0;
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
        if(item->pos != item->length &&
            !grammar_is_in_core_items(&grammar->item_sets, item, item->pos + 1) &&
            !grammar_is_in_core_items(item_sets, item, item->pos + 1))
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
        }

        item = boo_list_next(item);
    }

    memset(grammar->transition_lookup, 0, (grammar->num_symbols + UCHAR_MAX + 1) * sizeof(void*));

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

    root_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

    if(root_item_set == NULL) {
        return BOO_ERROR;
    }

    grammar_item_from_rule(root_item, root_rule);

    root_item->core = 1;

    boo_list_append(&root_item_set->items, &root_item->entry);

    return grammar_build_item_sets(grammar);
}

static void
grammar_dump_item(boo_lalr1_item_t *item) {
    boo_int_t i;

    printf(item->core ? "%x -> " : "+%x -> ", item->lhs);

    for(i = 0 ; i != item->length ; i++) {
        if(item->pos == i) {
            printf(". ");
        }

        printf("%x ", item->rhs[i]);
    }

    if(item->pos == i) {
        printf(". ");
    }

    printf("\n");
}

static void
grammar_dump_item_set(boo_lalr1_item_set_t *item_set)
{
    boo_lalr1_item_t *item;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {

        grammar_dump_item(item);

        item = boo_list_next(item);
    }
}
