
/*
 * This is bootstrap code. It is a simple and stupid fscanf-based
 * implementation of grammar definition parser.
 */

#include <stdio.h>
#include <limits.h>

#include "boo.h"
#include "pool.h"
#include "vector.h"
#include "grammar.h"

typedef enum {
    s_lhs, s_rhs
} parser_state_t;

symbol_t *bootstrap_add_symbol(boo_grammar_t *grammar, boo_vector_t *lhs_lookup, boo_str_t *token)
{
    symbol_t *symbol;
    boo_rule_t **lookup;

    symbol = symtab_resolve(grammar->symtab, token);

    if(symbol == NULL) {
        token->data = pstrdup(grammar->pool, token->data, token->len);

        if(token->data == NULL) {
            return NULL;
        }

        symbol = symtab_add(grammar->symtab, token, grammar->num_symbols + UCHAR_MAX + 1);

        if(symbol == NULL) {
            return NULL;
        }

        lookup = vector_append(lhs_lookup);

        if(lookup == NULL) {
            return NULL;
        }

        *lookup = NULL;

        grammar->num_symbols++;
    }

    return symbol;
}

boo_int_t bootstrap_parse_file(boo_grammar_t *grammar, pool_t *pool, boo_str_t *filename)
{
    FILE *fin;
    char token_buffer[128];
    boo_int_t rc;
    parser_state_t state;
    boo_str_t token;
    boo_uint_t lhs;
    boo_int_t has_lhs;
    boo_vector_t *rhs_vector;
    boo_vector_t *lhs_lookup;
    symbol_t *symbol;
    boo_rule_t *rule;
    boo_uint_t *rhs;
    boo_rule_t **lookup;

    state = s_lhs;
    has_lhs = 0;

    rhs_vector = vector_create(pool, sizeof(boo_uint_t), 8);

    if(rhs_vector == NULL) {
        return BOO_ERROR;
    }

    lhs_lookup = vector_create(pool, sizeof(void*), 256);

    if(lhs_lookup == NULL) {
        return BOO_ERROR;
    }

    fin = fopen((char*)filename->data, "r");

    if(fin == NULL) {
        return BOO_ERROR;
    }

    while(!feof(fin)) {

        rc = fscanf(fin, "%128s", token_buffer);

        if(rc == EOF && state == s_lhs && !has_lhs) {
            break;
        }

        if(rc != 1) {
            goto cleanup;
        }

        token.data = (u_char*)token_buffer;
        token.len = strlen(token_buffer);

        switch(state) {
            case s_lhs:
                if(token.len == 1 && token.data[0] == ':') {
                    state = s_rhs;
                }
                else if(!has_lhs) {
                    symbol = bootstrap_add_symbol(grammar, lhs_lookup, &token);

                    if(symbol == NULL) {
                        goto cleanup;
                    }

                    lhs = symbol->value;
                    has_lhs = 1;
                }
                else {
                    fprintf(stderr, "only one symbol is allowed on the left-hand side");
                    goto cleanup;
                }
                break;
            case s_rhs:
                if(token.len == 1) {
                    if(token.data[0] == '|' || token.data[0] == ';') {
                        rule = pcalloc(grammar->pool, sizeof(boo_rule_t));

                        if(rule == NULL) {
                            goto cleanup;
                        }

                        rule->length = rhs_vector->nelements;
                        rule->lhs = lhs;
                        rule->rhs = palloc(grammar->pool, sizeof(boo_uint_t));
                        rule->action = 0;

                        if(rule->rhs == NULL) {
                            goto cleanup;
                        }

                        memcpy(rule->rhs, rhs_vector->elements, rule->length * sizeof(boo_uint_t));

                        grammar_add_rule(grammar, rule);

                        lookup = lhs_lookup->elements;

                        lookup += (lhs - UCHAR_MAX - 1);

                        rule->lhs_hash_next = *lookup;
                        *lookup = rule;

                        if(token.data[0] == ';') {
                            state = s_lhs;
                            has_lhs = 0;
                        }

                        vector_clear(rhs_vector);
                    }
                }
                else {
                    rhs = vector_append(rhs_vector);

                    if(rhs == NULL) {
                        goto cleanup;
                    }

                    if(token.data[0] == '\'') {
                        *rhs = token.data[1] | BOO_TOKEN;
                    }
                    else {
                        symbol = bootstrap_add_symbol(grammar, lhs_lookup, &token);

                        if(symbol == NULL) {
                            goto cleanup;
                        }

                        *rhs = symbol->value;

                        /*
                         * Treat symbols in capitals as tokens
                         */
                        if(token.data[0] >= 'A' && token.data[0] <= 'Z') {
                            *rhs |= BOO_TOKEN;
                        }
                    }
                }
                
                break;
        }
    }

    /*
     * Copy left-hand side symbol lookup table to a permanent place
     */
    grammar->lhs_lookup = palloc(grammar->pool, grammar->num_symbols * sizeof(boo_rule_t*));    

    if(grammar->lhs_lookup == NULL) {
        goto cleanup;
    }

    memcpy(grammar->lhs_lookup, lhs_lookup->elements, grammar->num_symbols * sizeof(boo_rule_t*));

    grammar->transition_lookup = pcalloc(grammar->pool, (grammar->num_symbols + UCHAR_MAX + 1) * sizeof(boo_lalr1_item_set_t*));    

    if(grammar->transition_lookup == NULL) {
        goto cleanup;
    }

    fclose(fin);
    return BOO_OK;

cleanup:
    fclose(fin);
    return BOO_ERROR;
}
