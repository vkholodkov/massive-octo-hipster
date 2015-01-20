
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "boo.h"
#include "pool.h"

#define BOO_INFINITY UINT_MAX

void *new_alternative(pool_t*, void*, void*);
void *new_sequence(pool_t*, void*, void*);
void *new_repetition(pool_t*, void*, int, int);
void *new_any(pool_t*);
void *new_sol(pool_t*);
void *new_eol(pool_t*);
void *new_set(pool_t*);
void *new_range(pool_t*, void*, void*);
void *new_char(pool_t*, char);

#include "regex.h"

static const char *input = "[\\w*]\\.(bat|cmd|vbs|wsh|vbe|wsf|hta)[\\W]{0,}$";
//static const char *input = "/web/magmi_([a-z]*).php";
//static const char *input = "<!ENTITY(\\s+)(%*\\s*)([a-zA-Z1-9_-]*)(\\s+)SYSTEM";

static void
print_rule(boo_uint_t rule)
{
    int i;

    printf("%s <- ", boo_symbol[boo_lhs[rule]]);

    for(i = 0 ; i != boo_rule[rule << 1] ; i++) {
        printf("%s ", boo_symbol[boo_rhs[boo_rule[(rule << 1) + 1] + i]]);
    }

    printf("\n");
}

int main(int argc, char *argv[]) {
    const u_char *p, *q;
    boo_uint_t base, rule, lhs;
    boo_int_t next;
    boo_stack_elm_t stack[256], *top;
    pool_t *pool = NULL;

    p = (const u_char*)input;
    q = p + strlen(input);

    top = stack;

    top->state = 0;

    for(;;) {
        if(p != q) {
            if(*p >= ' ') {
                printf("state %d input '%c'\n", top->state, *p);
            }
            else {
                printf("state %d input %d\n", top->state, *p);
            }

            base = boo_t_base[top->state] + *p;

            if(boo_t_check[base] != top->state) {
                base = boo_t_base[top->state] + BOO_CHAR;
            }
        }
        else {
            printf("state %d input $eof\n", top->state);
            base = boo_t_base[top->state] + BOO_EOF;
        }

        if(boo_t_check[base] != top->state) {
            fprintf(stderr, "syntax error\n");
            break;
        }

        next = boo_t_next[base];

        if(next > 0) {
            printf("shift %d\n", next);
            top++; top->state = next; p++;
        }
        else if(next < 0) {
            rule = -next;

            printf("reduce %d: ", rule);
            print_rule(rule);

            boo_action(rule, pool, top);

            top -= boo_rule[rule << 1];

            lhs = boo_lhs[rule];

            base = boo_nt_base[top->state] + lhs;

            if(boo_nt_check[base] == top->state) {
                printf("\nstate %d input %s\n", top->state, boo_symbol[lhs]);
                next = boo_nt_next[base];
                top++; top->state = next;
                printf("shift %d\n", next);
            }
        }
        else {
            printf("accepted\n");
            break;
        }

        printf("\n");
    }

    return 0;
}

void *new_alternative(pool_t *pool, void *left, void *right)
{
    return NULL;
}

void *new_sequence(pool_t *pool, void *before, void *after)
{
    return NULL;
}

void *new_repetition(pool_t *pool, void *pattern, int min, int max)
{
    return NULL;
}

void *new_any(pool_t *pool)
{
    return NULL;
}

void *new_sol(pool_t *pool)
{
    return NULL;
}

void *new_eol(pool_t *pool)
{
    return NULL;
}

void *new_set(pool_t *pool)
{
    return NULL;
}

void *new_range(pool_t *pool, void *start, void *end)
{
    return NULL;
}

void *new_char(pool_t *pool, char c)
{
    return NULL;
}

