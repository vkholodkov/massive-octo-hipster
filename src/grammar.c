
#include <stdio.h>

#include "grammar.h"

static const boo_str_t accept_symbol_name = boo_string("$accept");

void grammar_dump_item_set(FILE*, boo_grammar_t*, boo_lalr1_item_set_t*);

/*
 * Create a grammar and initialize all internal lists and a symbol table
 */
boo_grammar_t *grammar_create(pool_t *p)
{
    boo_grammar_t *grammar;

    grammar = pcalloc(p, sizeof(boo_grammar_t));

    if(grammar == NULL) {
        return NULL;
    }

    grammar->pool = p;

    boo_list_init(&grammar->types);
    boo_list_init(&grammar->rules);
    boo_list_init(&grammar->item_sets);
    boo_list_init(&grammar->reverse_item_sets);
    boo_list_init(&grammar->reductions);

    grammar->symtab = symtab_create(p);

    if(grammar->symtab == NULL) {
        return NULL;
    }

    grammar->lookahead_set = NULL;

    grammar->core_set_index = tree_create(p);

    if(grammar->core_set_index == NULL) {
        return NULL;
    }

    return grammar;
}

static boo_str_t boo_context_str = boo_string("void");

boo_int_t grammar_wrapup(boo_grammar_t *grammar) {
    boo_int_t i;
    boo_lhs_lookup_t *lhs_lookup;
    symbol_t *symbol;
    boo_rule_t *rule;

    grammar->transition_lookup = pcalloc(grammar->pool,
        boo_symbol_to_code(grammar->num_symbols) * sizeof(boo_trans_lookup_t));    

    if(grammar->transition_lookup == NULL) {
        return BOO_ERROR;
    }

    if(grammar->context == NULL) {
        grammar->context = &boo_context_str;
    }

    memset(grammar->transition_lookup, 0,
        boo_symbol_to_code(grammar->num_symbols) * sizeof(boo_trans_lookup_t));

    grammar->first_used_trans = NULL;

    lhs_lookup = grammar->lhs_lookup;

    for(i = 0 ; i != grammar->num_symbols + 1 ; i++) {
        if(lhs_lookup[i].rules == NULL) {
            symbol = symtab_resolve(grammar->symtab, &grammar->lhs_lookup[i].name);

            if(symbol != NULL && !boo_is_token(symbol->value)) {
                fprintf(stderr, "non-terminal %s (%u) is used in right-hand side but has no rules\n",
                    grammar->lhs_lookup[i].name.data, symbol->line);
                return BOO_ERROR;
            }
        }
    }

    rule = boo_list_begin(&grammar->rules);

    while(rule != boo_list_end(&grammar->rules)) {
        fprintf(grammar->debug, "%d) ", rule->rule_n);
        grammar_dump_rule(grammar->debug, grammar, rule);
        fprintf(grammar->debug, "\n");
        rule = boo_list_next(rule);
    }

    return BOO_OK;
}

/*
 * Add rule to a grammar. Nothing sophisticated.
 */
void grammar_add_rule(boo_grammar_t *grammar, boo_rule_t *rule)
{
    rule->rule_n = grammar->num_rules++;

    if(rule->action != NULL) {
        rule->action->rule_n = rule->rule_n;
        rule->action->rule_length = rule->length;
    }

    boo_list_append(&grammar->rules, &rule->entry);
}

/*
 * Convert a rule into an LR item
 */
static void
grammar_item_from_rule(boo_lalr1_item_t *item, boo_rule_t *rule)
{
    item->rule_n = rule->rule_n;
    item->lhs = rule->lhs;
    item->rhs = rule->rhs;
    item->length = rule->length;
    item->pos = 0;
    item->transition = NULL;
}

/*
 * Allocate an LALR(1) item set
 */
boo_lalr1_item_set_t*
grammar_alloc_item_set(boo_grammar_t *grammar)
{
    boo_free_item_set_t *is;

    if(grammar->reusable_item_sets != NULL) {
        is = grammar->reusable_item_sets;
        grammar->reusable_item_sets = is->next;
        return (boo_lalr1_item_set_t*)is;
    }

    is = pcalloc(grammar->pool, sizeof(boo_lalr1_item_set_t));

    if(is == NULL) {
        return NULL;
    }

    return (boo_lalr1_item_set_t*)is;
}

/*
 * Free an item set
 */
void grammar_free_item_set(boo_grammar_t *grammar, boo_lalr1_item_set_t *s)
{
    s->transitions = NULL;
    boo_free_item_set_t *is = (boo_free_item_set_t*)s;
    is->next = grammar->reusable_item_sets;
    grammar->reusable_item_sets = is;
}

/*
 * Close an item set:
 * compute a transitive closure of core set items
 */
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

        item = boo_list_end(&item_set->items);

        while(item != boo_list_begin(&item_set->items)) {
            item = boo_list_prev(item);

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

//            item = boo_list_next(item);
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

/*
 * Close all item sets
 */
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

#if 0
        fprintf(grammar->debug, "\nClosed item set:\n--------------------------\n");
        grammar_dump_item_set(grammar->debug, grammar, item_set);
#endif

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

void
grammar_item_set_add_item(boo_lalr1_item_set_t *item_set, boo_lalr1_item_t *n)
{
    boo_lalr1_item_t *item;
    int cmp, i;
    
    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {
        if(n->lhs < item->lhs || (item->lhs == n->lhs && n->length < item->length)) {
            break;
        }

        if(item->lhs == n->lhs && item->length == n->length) {
            cmp = 0;

            for(i = 0 ; i != item->length ; i++) {
                if(n->rhs[i] != item->rhs[i]) {
                    cmp = (boo_token_get(n->rhs[i]) < boo_token_get(item->rhs[i])) ? -1 : 1;
                    break;
                }
            }

            if(cmp < 0) {
                break;
            }

            if(cmp == 0 && n->pos < item->pos) {
                break;
            }
        } 

        item = boo_list_next(item);
    }

    boo_list_insert(&item_set->items, &n->entry, &item->entry);
}

/*
 * Returns true if core sets match
 */
boo_int_t grammar_core_sets_match(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set1, boo_lalr1_item_set_t *item_set2)
{
    boo_lalr1_item_t *item1 = boo_list_begin(&item_set1->items);
    boo_lalr1_item_t *item2 = boo_list_begin(&item_set2->items);

    while(item1 != boo_list_end(&item_set1->items)) {
        if(item1->core) {
            while(!item2->core && item2 != boo_list_end(&item_set2->items)) {
                item2 = boo_list_next(item2);
            }

            if(item2 == boo_list_end(&item_set2->items)) {
                return 0;
            }

            if(item1->lhs != item2->lhs || item1->length != item2->length || 
                item1->pos != item2->pos || memcmp(item1->rhs, item2->rhs, item1->length * sizeof(boo_uint_t)))
            {
                return 0;
            }

            item2 = boo_list_next(item2);
        }

        item1 = boo_list_next(item1);
    }

    if(item2 != boo_list_end(&item_set2->items)) {
        return 0;
    }
#if 0
    fprintf(grammar->debug, "\nCore sets:\n--------------------------\n");
    grammar_dump_item_set(grammar->debug, grammar, item_set1);
    fprintf(grammar->debug, "-------- AND ----------\n");
    grammar_dump_item_set(grammar->debug, grammar, item_set2);
    fprintf(grammar->debug, "-------- MATCH ----------\n");
#endif

    return 1;
}

static boo_int_t
grammar_remove_duplicate_core_sets(boo_grammar_t *grammar, boo_list_t *item_sets, boo_list_t *result_sets)
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
                fprintf(grammar->debug, "Removing duplicate core set:\n");
                grammar_dump_item_set(grammar->debug, grammar, item_set2);
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
grammar_find_transitions(boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set, boo_list_t *item_sets, boo_list_t *result_set)
{
    boo_uint_t symbol;
    boo_lalr1_item_t *item, *new_item;
    boo_lalr1_item_set_t *new_item_set;
    boo_trans_lookup_t *lookup;
    boo_transition_t *t;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {
        if(item->pos != item->length && boo_token_get(item->rhs[item->pos]) != BOO_EOF)
        {
#if 0
            if(item->pos + 1 != item->length && boo_token_get(item->rhs[item->pos + 1]) == BOO_EOF)
            {
                printf("skipping item with $eof after marker\n");
                item = boo_list_next(item);
                continue;
            }
#endif
            symbol = boo_token_get(item->rhs[item->pos]);
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

                /*
                 * Allocate and link a new transition
                 */
                t = pcalloc(grammar->pool, sizeof(boo_transition_t));

                if(t == NULL) {
                    return BOO_ERROR;
                }

                t->item_set = new_item_set;
                t->next = new_item_set->transitions;
                new_item_set->transitions = t;

                lookup->transition = t;
            }

            new_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

            if(new_item == NULL) {
                return BOO_ERROR;
            }

            new_item->rule_n = item->rule_n;
            new_item->lhs = item->lhs;
            new_item->length = item->length;
            new_item->rhs = item->rhs;
            new_item->pos = item->pos + 1;
            new_item->core = 1;

            grammar_item_set_add_item(new_item_set, new_item);

            if(new_item->pos == new_item->length) {
                new_item_set->has_reductions = 1;
            }

            item->transition = lookup->transition;
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

static void
grammar_renumber_item_sets(boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_uint_t n = 0;

    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        item_set->state_n = n++;

        item_set = boo_list_next(item_set);
    }

    grammar->num_item_sets = n;
}

/*
 * Add an item to core set index
 */
static boo_int_t
grammar_add_item_to_core_set_index(boo_grammar_t *grammar, boo_lalr1_item_t *item, boo_uint_t *state_n)
{
    boo_trie_node_t *n;
    boo_trie_t *t = grammar->core_set_index;
    boo_uint_t *p, *q, symbol;

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
     * Add an element corresponding to the position
     */
    n = boo_trie_add_sequence(t, n, &item->pos, &item->pos + 1);

    if(n == NULL) {
        return BOO_ERROR;
    }

    /*
     * We've reached a leaf of the trie,
     * set it to the state number
     */
    n->leaf = state_n;

    return BOO_OK;
}

static boo_int_t
grammar_add_to_core_set_index(boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_int_t rc;
    boo_lalr1_item_set_t *item_set;
    boo_lalr1_item_t *item;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        item = boo_list_begin(&item_set->items);

        while(item != boo_list_end(&item_set->items)) {

            if(item->core) {
                rc = grammar_add_item_to_core_set_index(grammar, item, &item_set->state_n);

                if(rc != BOO_OK) {
                    return rc;
                }
            }

            item = boo_list_next(item);
        }

        item_set = boo_list_next(item_set);
    }

    return BOO_OK;
}

static boo_int_t
grammar_build_item_sets(boo_grammar_t *grammar, boo_list_t *dest)
{
    boo_int_t rc;
    boo_list_t to_process, add_queue;

    boo_list_init(&to_process);
    boo_list_init(&add_queue);

    rc = grammar_close_item_sets(grammar, dest);

    if(rc == BOO_ERROR) {
        return BOO_ERROR;
    }

    rc = grammar_find_transitions_for_set(grammar, dest, &add_queue);

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

        boo_list_splice(dest, &to_process);

        rc = grammar_remove_duplicate_core_sets(grammar, dest, &add_queue);

        if(rc == BOO_ERROR) {
            return BOO_ERROR;
        }
    } while(!boo_list_empty(&add_queue));

    grammar_renumber_item_sets(grammar, dest);

    return grammar_add_to_core_set_index(grammar, dest);
}

boo_int_t grammar_generate_lr_item_sets(boo_grammar_t *grammar, boo_list_t *dest)
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

    root_rule = grammar->lhs_lookup[boo_code_to_symbol(grammar->root_symbol)].rules;

    if(root_rule == NULL) {
        fprintf(stderr, "cannot resolve root rule\n");
        return BOO_ERROR;
    }

    while(root_rule != NULL) { 
        root_item = pcalloc(grammar->pool, sizeof(boo_lalr1_item_t));

        if(root_item_set == NULL) {
            return BOO_ERROR;
        }

        grammar_item_from_rule(root_item, root_rule);

        root_item->core = 1;

        boo_list_append(&root_item_set->items, &root_item->entry);

        root_rule = root_rule->lhs_hash_next;
    }

    return grammar_build_item_sets(grammar, dest);
}

boo_int_t grammar_generate_lookahead_sets(boo_grammar_t *grammar)
{
    return BOO_OK;
}

void grammar_dump_rule(FILE *out, boo_grammar_t *grammar, boo_rule_t *rule)
{
    boo_int_t i;

    boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(rule->lhs)].name);
    fprintf(out, " -> ");

    for(i = 0 ; i != rule->length ; i++) {
        if(boo_is_token(rule->rhs[i])) {
            if(boo_token_get(rule->rhs[i]) == BOO_EOF) {
                fprintf(out, "$eof ");
            }
            else {
                boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(rule->rhs[i])].name);
                fputc(' ', out);
            }
        }
        else {
            boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(rule->rhs[i])].name);
            fputc(' ', out);
        }
    }
}

void grammar_dump_rule_from_item(FILE *out, boo_grammar_t *grammar, boo_lalr1_item_t *item)
{
    boo_int_t i;

    boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->lhs)].name);
    fprintf(out, " -> ");

    for(i = 0 ; i != item->length ; i++) {
        if(boo_is_token(item->rhs[i])) {
            if(boo_token_get(item->rhs[i]) == BOO_EOF) {
                fprintf(out, "$eof ");
            }
            else {
                boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->rhs[i])].name);
                fputc(' ', out);
            }
        }
        else {
            boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->rhs[i])].name);
            fputc(' ', out);
        }
    }
}

void grammar_dump_item(FILE *out, boo_grammar_t *grammar, boo_lalr1_item_t *item) {
    boo_int_t i;

    fprintf(out, item->core ? "" : "+");
    boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->lhs)].name);
    fprintf(out, " -> ");

    for(i = 0 ; i != item->length ; i++) {
        if(item->pos == i) {
            fprintf(out, ". ");
        }

        if(boo_is_token(item->rhs[i])) {
            if(boo_token_get(item->rhs[i]) == BOO_EOF) {
                fprintf(out, "$eof ");
            }
            else {
                boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->rhs[i])].name);
                fputc(' ', out);
            }
        }
        else {
            boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->rhs[i])].name);
            fputc(' ', out);
        }
    }

    if(item->pos == i) {
        fprintf(out, ". ");
    }

    if(item->transition != NULL) {
        fprintf(out, " -- goto %d", item->transition->item_set->state_n);
    }
#if 0
    if(item->remove != 0) {
        fprintf(out, " remove %d", item->remove);
    }

    if(item->original_symbol != 0) {
        fprintf(out, " original ");
        boo_puts(out, &grammar->lhs_lookup[boo_code_to_symbol(item->original_symbol)].name);
    }
#endif
    fprintf(out, "\n");
}

void grammar_dump_item_set(FILE *out, boo_grammar_t *grammar, boo_lalr1_item_set_t *item_set)
{
    boo_lalr1_item_t *item;

    item = boo_list_begin(&item_set->items);

    while(item != boo_list_end(&item_set->items)) {

        grammar_dump_item(out, grammar, item);

        item = boo_list_next(item);
    }
}

void grammar_dump_item_sets(FILE *out, boo_grammar_t *grammar, boo_list_t *item_sets)
{
    boo_uint_t item_set_n = 0;
    boo_lalr1_item_set_t *item_set;

    item_set = boo_list_begin(item_sets);

    while(item_set != boo_list_end(item_sets)) {

        fprintf(out, "Item set %d (state %d):\n", item_set_n, item_set->state_n);

        grammar_dump_item_set(out, grammar, item_set);

        fprintf(out, "\n");

        item_set = boo_list_next(item_set);
        item_set_n++;
    }

    fflush(out);
}
