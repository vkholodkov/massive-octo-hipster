
#ifndef _LOOKAHEAD_H_
#define _LOOKAHEAD_H_

#include "boo.h"
#include "grammar.h"

boo_int_t lookahead_generate_item_sets(boo_grammar_t*, boo_list_t*);
boo_int_t lookahead_build(boo_grammar_t*, boo_list_t*);

#endif
