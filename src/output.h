
#ifndef _BOO_OUTPUT_H_
#define _BOO_OUTPUT_H_

#include "grammar.h"
#include "darray.h"

typedef struct {
    darray_t        *darray;
    boo_uint_t      max_cells;
    boo_uint_t      row_stride;
} boo_output_t;

boo_output_t *output_create(pool_t*);
boo_int_t output_add_grammar(boo_output_t*, boo_grammar_t*);
boo_int_t output_base(boo_output_t*, boo_grammar_t*);
boo_int_t output_action(boo_output_t*, boo_grammar_t*);
boo_int_t output_check(boo_output_t*, boo_grammar_t*);

#endif
