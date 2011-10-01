
#include "string.h"

u_char *boo_strcpy(u_char *p,boo_str_t *s) {
    memcpy(p, s->data, s->len);

    return p + s->len;
}

int boo_const_strequ(boo_str_t *s1, const char *s2) {
    u_char      *p, *q;

    p = s1->data;
    q = p + s1->len;

    while(p != q) {
        if(*s2 == '\0' || *p != *s2) {
            return 0;
        }

        s2++;
        p++;
    }

    return 1;
}
