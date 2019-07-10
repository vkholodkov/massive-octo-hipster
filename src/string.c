
#include <ctype.h>

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

void boo_puts(FILE *out, boo_str_t *s) {
    fwrite(s->data, s->len, 1, out);
}

void boo_puts_upper(FILE *out, boo_str_t *s) {
    u_char *p, *q;

    p = s->data;
    q = p + s->len;

    while(p != q) {
        fputc(toupper(*p), out);
        p++;
    }
}

void boo_puts_lower(FILE *out, boo_str_t *s) {
    u_char *p, *q;

    p = s->data;
    q = p + s->len;

    while(p != q) {
        fputc(tolower(*p), out);
        p++;
    }
}

void boo_escape_puts(FILE *out, boo_str_t *s)
{
    u_char *p, *q;

    p = s->data;
    q = p + s->len;

    while(p != q) {
        switch(*p) {
            case '\a': fputc('\\', out); fputc('a', out); break;
            case '\b': fputc('\\', out); fputc('b', out); break;
            case '\f': fputc('\\', out); fputc('f', out); break;
            case '\n': fputc('\\', out); fputc('n', out); break;
            case '\r': fputc('\\', out); fputc('r', out); break;
            case '\t': fputc('\\', out); fputc('t', out); break;
            case '\v': fputc('\\', out); fputc('v', out); break;
            case '\\': fputc('\\', out); fputc('\\', out); break;
            case '\"': fputc('\\', out); fputc('"', out); break;
            default:
                if(*p < ' ') {
                    fprintf(out, "\\%x", *p);
                }
                else {
                    fputc(*p, out);
                }
                break; 
        }
        p++;
    }
}
