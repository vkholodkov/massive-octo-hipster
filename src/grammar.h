
#ifndef _BOO_GRAMMAR_H_
#define _BOO_GRAMMAR_H_

#include <limits.h>

#include "pool.h"
#include "list.h"
#include "symtab.h"

#define         BOO_TOKEN               0x80000000
#define         BOO_EOF                 0
#define         BOO_START               1

#define         boo_code_to_symbol(s)   (((s) & ~BOO_TOKEN) - BOO_START)
#define         boo_symbol_to_code(s)   ((s) + BOO_START)
#define         boo_token_get(x)        ((x) & ~BOO_TOKEN)

#define boo_is_token(x) (((x) & BOO_TOKEN) != 0)

struct boo_lalr1_item_set_s;

typedef struct {
    boo_str_t               name;
    void                    *rules;
} boo_lhs_lookup_t;

typedef struct boo_transition_s {
    struct boo_lalr1_item_set_s     *item_set;
    struct boo_transition_s         *next;
} boo_transition_t;

typedef struct boo_trans_lookup_s {
    struct boo_lalr1_item_set_s     *item_set;
    struct boo_trans_lookup_s       *next;
    boo_transition_t                *transition;
} boo_trans_lookup_t;

typedef struct boo_free_item_set_s {
    struct boo_free_item_set_s *next;
} boo_free_item_set_t;

typedef struct {
    pool_t                  *pool;
    symtab_t                *symtab;
    boo_list_t              rules;
    boo_list_t              actions;
    boo_list_t              item_sets;
    boo_list_t              reverse_item_sets;

    boo_uint_t              num_item_sets;
    boo_uint_t              num_symbols;

    boo_lhs_lookup_t        *lhs_lookup;
    boo_trans_lookup_t      *transition_lookup;
    boo_trans_lookup_t      *first_used_trans;

    void                    *reusable_item_sets;
} boo_grammar_t;

typedef struct boo_rule_s {
    boo_list_entry_t        entry;

    boo_uint_t              length;
    boo_uint_t              lhs;
    boo_uint_t              *rhs;

    boo_uint_t              action;

    struct boo_rule_s       *lhs_hash_next;
    void                    *booked_by_item_set;
} boo_rule_t;

typedef struct {
    boo_list_entry_t        entry;

    /*
     * Start and end offsets of the action code in the source file
     */
    off_t                   start, end;
} boo_action_t;

typedef struct boo_lalr1_item_set_s {
    boo_list_entry_t        entry;
    boo_list_t              items;

    /*
     * Transitions that point to this item
     */
    boo_transition_t        *transitions;

    boo_uint_t              state_n;

    unsigned                closed:1;
    unsigned                has_reductions:1;
} boo_lalr1_item_set_t;

typedef struct {
    boo_list_entry_t        entry;

    boo_uint_t              length;
    boo_uint_t              pos;
    boo_uint_t              lhs;
    boo_uint_t              *rhs; 

    boo_transition_t        *transition;

    unsigned                closed:1;
    unsigned                core:1;
} boo_lalr1_item_t;

#define grammar_lookup_rules_by_lhs(grammar,symbol) (boo_rule_t*)grammar->lhs_lookup[boo_code_to_symbol(symbol)].rules

boo_grammar_t *grammar_create(pool_t*);
boo_int_t grammar_wrapup(boo_grammar_t*);
void grammar_add_rule(boo_grammar_t*, boo_rule_t*);
boo_int_t grammar_generate_lr_item_sets(boo_grammar_t*, boo_list_t*);
void grammar_dump_item_sets(boo_grammar_t*, boo_list_t*);

#endif
