
#ifndef _BOO_OUTPUT_H_
#define _BOO_OUTPUT_H_

#include "grammar.h"

typedef struct gap {
    boo_uint_t from, to;
    struct gap *next;
} boo_gap_t;

typedef struct boo_output_transition {
    u_char                              input;
    boo_int_t                           target;
    struct boo_output_transition        *next;
} boo_output_transition_t;

typedef struct {
    boo_int_t                           base;
    boo_output_transition_t             *transitions;
} boo_output_state_t;

typedef struct {
    pool_t              *pool;
    boo_uint_t          max_cells, row_stride, num_states, end;
    boo_gap_t           *gaps, *free_gaps;
    boo_output_state_t  *states;
    u_char              *next;
    FILE                *file, *debug;
} boo_output_t;

boo_output_t *output_create(pool_t*);
boo_int_t output_add_grammar(boo_output_t*, boo_grammar_t*);
boo_int_t output_base(boo_output_t*, boo_grammar_t*);
boo_int_t output_action(boo_output_t*, boo_grammar_t*);
boo_int_t output_check(boo_output_t*, boo_grammar_t*);

#endif
