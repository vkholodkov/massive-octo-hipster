
#ifndef _STRING_
#define _STRING_

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    u_char              *data;
    size_t              len;
} boo_str_t;

#define boo_string(x) { (u_char*)(x), sizeof(x) - 1 }
#define boo_null_string { 0, 0 }

#define boo_strempty(x) ((x)->len == 0)
#define boo_strcmp(x, y) ((x)->len == (y)->len ? memcmp((x)->data, (y)->data, (x)->len) : 1)
#define boo_strequ(x, y) ((x)->len == (y)->len ? memcmp((x)->data, (y)->data, (x)->len) == 0 : 0)

u_char *boo_strcpy(u_char*,boo_str_t*);
int boo_const_strequ(boo_str_t*, const char*);

void boo_puts(FILE*,boo_str_t*);
void boo_puts_upper(FILE*,boo_str_t*);
void boo_puts_lower(FILE*,boo_str_t*);
void boo_escape_puts(FILE*,boo_str_t*);

#endif //_STRING_

