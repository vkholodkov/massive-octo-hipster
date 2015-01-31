
#ifndef _BOO_OUTPUT_H_
#define _BOO_OUTPUT_H_

#include "grammar.h"
#include "lookup.h"

typedef struct {
    pool_t              *pool;
    boo_uint_t          max_cells, row_stride, num_states;
    boo_lookup_table_t  *term, *nterm;
    FILE                *file, *debug;
} boo_output_t;

boo_output_t *output_create(pool_t*);
boo_int_t output_add_grammar(boo_output_t*, boo_grammar_t*);
boo_int_t output_codes(boo_output_t*, boo_grammar_t*);
boo_int_t output_symbols(boo_output_t*, boo_grammar_t*);
boo_int_t output_lookup(boo_output_t*, boo_grammar_t*);
boo_int_t output_actions(boo_output_t*, boo_grammar_t*, const char*);
boo_int_t output_lhs(boo_output_t*, boo_grammar_t*);
boo_int_t output_rhs(boo_output_t*, boo_grammar_t*);
boo_int_t output_rules(boo_output_t*, boo_grammar_t*);

#endif
