
#ifndef _BOO_LOOKUP_H_
#define _BOO_LOOKUP_H_

#include <stdio.h>

#include "boo.h"
#include "pool.h"

typedef struct gap {
    boo_uint_t from, to;
    struct gap *next;
} boo_gap_t;

typedef struct boo_lookup_transition {
    boo_uint_t                          input;
    boo_int_t                           target;
    struct boo_lookup_transition        *next;
} boo_lookup_transition_t;

typedef struct {
    boo_int_t                           base;
    boo_lookup_transition_t             *transitions;
} boo_lookup_state_t;

typedef struct {
    pool_t              *pool;
    FILE                *debug;
    boo_gap_t           *gaps, *free_gaps;
    boo_lookup_state_t  *states;
    u_char              *next;
    boo_uint_t          row_stride, num_states, end;
} boo_lookup_table_t;

boo_lookup_table_t *lookup_create(pool_t*, boo_uint_t);
boo_int_t lookup_index(boo_lookup_table_t*);
boo_int_t lookup_write(FILE*, boo_lookup_table_t*, const char*);
boo_int_t lookup_add_transition(boo_lookup_table_t*, boo_uint_t, boo_uint_t, boo_int_t);

#endif
