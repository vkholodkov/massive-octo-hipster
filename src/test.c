
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "boo.h"
#include "regex.h"

//static const char *input = "[\\w*]\\.(bat|cmd|vbs|wsh|vbe|wsf|hta)[\\W]{0,}$";
static const char *input = "/web/magmi_([a-z]*).php";
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
    boo_uint_t base, action, num_actions, lhs, remove;
    boo_int_t next;
    __attribute__((__unused__)) const char *sym = boo_symbol[0];
    boo_uint_t stack[256], *top;

    p = (const u_char*)input;
    q = p + strlen(input);

    top = stack;

    *top = 0;

    for(;;) {
        if(p != q) {
            if(*p >= ' ') {
                printf("state %d input '%c'\n", *top, *p);
            }
            else {
                printf("state %d input %d\n", *top, *p);
            }

            base = boo_t_base[*top] + *p;
        }
        else {
            printf("state %d input $eof\n", *top);
            base = boo_t_base[*top] + BOO_EOF;
        }

        if(boo_t_check[base] != *top) {
            base = boo_t_base[*top] + BOO_CHAR;
        }

        if(boo_t_check[base] != *top) {
            fprintf(stderr, "syntax error\n");
            break;
        }

        next = boo_t_next[base];

        if(next > 0) {
            printf("shift %d\n", next);
            *++top = next; p++;
        }
        else if(next < 0) {
            action = -next;
            num_actions = boo_action[action++];
            remove = boo_rule[boo_action[action] << 1];

            while(num_actions--) {
                printf("reduce %d: ", boo_action[action]);
                print_rule(boo_action[action]);

                top -= remove;

                lhs = boo_lhs[boo_action[action]];

                base = boo_nt_base[*top] + lhs;

                if(boo_nt_check[base] == *top) {
                    printf("\nstate %d input %s\n", *top, boo_symbol[lhs]);
                    next = boo_nt_next[base];
                    *++top = next;
                    printf("shift %d\n", next);
                    break;
                }

                action++;

                remove -= 1; // LHS
                remove += boo_rule[boo_action[action] << 1]; // RHS
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
