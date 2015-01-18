
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

#define BOO_LITERAL             1

typedef enum {
    s_lhs, s_rhs
} parser_state_t;

static symbol_t *
bootstrap_add_symbol(boo_grammar_t *grammar, boo_vector_t *lhs_lookup, boo_str_t *token, boo_uint_t flags)
{
    symbol_t *symbol;
    boo_lhs_lookup_t *lookup;
    boo_uint_t i, t;

    symbol = symtab_resolve(grammar->symtab, token);

    if(symbol == NULL) {
        token->data = pstrdup(grammar->pool, token->data, token->len);

        if(token->data == NULL) {
            return NULL;
        }

        symbol = symtab_add(grammar->symtab, token, boo_symbol_to_code(grammar->num_symbols));

        if(symbol == NULL) {
            return NULL;
        }

        /*
         * Treat symbols in capitals as tokens
         */
        t = 1;
        for(i = 0 ; i != token->len ; i++) {
            if(token->data[i] >= 'a' && token->data[i] <= 'z') {
                t = 0;
            }
        }

        if(t || (flags & BOO_LITERAL)) {
            symbol->value |= BOO_TOKEN;
        }

        lookup = vector_append(lhs_lookup);

        if(lookup == NULL) {
            return NULL;
        }

        lookup->rules = NULL;
        lookup->name = symbol->name;
        lookup->literal = (flags & BOO_LITERAL) ? 1 : 0;
        lookup->token = (symbol->value & BOO_TOKEN) ? 1 : 0;

        grammar->num_symbols++;
    }

    return symbol;
}

static boo_int_t
bootstrap_add_accept_symbol(boo_vector_t *rhs_vector)
{
    boo_uint_t *rhs;

    rhs = vector_append(rhs_vector);

    if(rhs == NULL) {
        return BOO_ERROR;
    }

    *rhs = BOO_EOF | BOO_TOKEN;

    return BOO_OK;
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
    boo_lhs_lookup_t *lookup;
    boo_uint_t flags;
    boo_uint_t rule_no;

    state = s_lhs;
    has_lhs = 0;
    lhs = 0;
    rule_no = 1;

    rhs_vector = vector_create(pool, sizeof(boo_uint_t), 8);

    if(rhs_vector == NULL) {
        return BOO_ERROR;
    }

    lhs_lookup = vector_create(pool, sizeof(boo_lhs_lookup_t), (UCHAR_MAX + 1) * 2);

    if(lhs_lookup == NULL) {
        return BOO_ERROR;
    }

    fin = fopen((char*)filename->data, "r");

    if(fin == NULL) {
        fprintf(stderr, "cannot open input file %s", filename->data);
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
                    symbol = bootstrap_add_symbol(grammar, lhs_lookup, &token, 0);

                    if(symbol == NULL) {
                        goto cleanup;
                    }

                    symbol->line = rule_no++;

                    lhs = symbol->value;
                    has_lhs = 1;
                }
                else {
                    fprintf(stderr, "only one symbol is allowed on the left-hand side");
                    goto cleanup;
                }
                break;
            case s_rhs:
                if(token.len == 1 && (token.data[0] == '|' || token.data[0] == ';')) {
                    if(lhs == BOO_START) {
                        if(bootstrap_add_accept_symbol(rhs_vector) != BOO_OK)
                        {
                            goto cleanup;
                        }
                    }

                    if(rhs_vector->nelements == 0) {
                        fprintf(stderr, "zero length rule\n");
                        goto cleanup;
                    }

                    rule = pcalloc(grammar->pool, sizeof(boo_rule_t));

                    if(rule == NULL) {
                        goto cleanup;
                    }

                    rule->length = rhs_vector->nelements;
                    rule->lhs = lhs;
                    rule->rhs = palloc(grammar->pool, rule->length * sizeof(boo_uint_t));
                    rule->action = 0;

                    if(rule->rhs == NULL) {
                        goto cleanup;
                    }

                    memcpy(rule->rhs, rhs_vector->elements, rule->length * sizeof(boo_uint_t));

                    grammar_add_rule(grammar, rule);

                    lookup = lhs_lookup->elements;

                    lookup += boo_code_to_symbol(lhs);

                    rule->lhs_hash_next = lookup->rules;
                    lookup->rules = rule;

                    if(token.data[0] == ';') {
                        state = s_lhs;
                        has_lhs = 0;
                    }

                    vector_clear(rhs_vector);
                }
                else {
                    flags = 0;

                    rhs = vector_append(rhs_vector);

                    if(rhs == NULL) {
                        goto cleanup;
                    }

                    if(token.data[0] == '\'') {
                        if(token.len != 3) {
                            fprintf(stderr, "Invalid token");
                            goto cleanup;
                        }

                        token.data++;
                        token.len -= 2;

                        flags = BOO_LITERAL;
                    }

                    symbol = bootstrap_add_symbol(grammar, lhs_lookup, &token, flags);

                    if(symbol == NULL) {
                        goto cleanup;
                    }

                    *rhs = symbol->value;
                }
                
                break;
        }
    }

    /*
     * Copy left-hand side symbol lookup table to a permanent place
     */
    grammar->lhs_lookup = palloc(grammar->pool, (grammar->num_symbols + 1) * sizeof(boo_lhs_lookup_t));    

    if(grammar->lhs_lookup == NULL) {
        goto cleanup;
    }

    memcpy(grammar->lhs_lookup, lhs_lookup->elements, (grammar->num_symbols + 1) * sizeof(boo_lhs_lookup_t));

    fclose(fin);
    return BOO_OK;

cleanup:
    fclose(fin);
    return BOO_ERROR;
}
