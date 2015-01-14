
#ifndef _SYMTAB_
#define _SYMTAB_

#include "string.h"
#include "boo.h"

typedef struct symbol {
    struct symbol       *next;
    boo_str_t           name;
    boo_uint_t          value;
    boo_uint_t          line;
    ssize_t             offset;
} symbol_t;

typedef struct {
    pool_t              *pool;
    
    symbol_t            **hashtab;
} symtab_t;

extern symtab_t     *global_symtab;

symtab_t *symtab_create(pool_t *pool);
symbol_t *symtab_resolve(symtab_t *symtab, boo_str_t *name);
symbol_t *symtab_add(symtab_t *symtab, boo_str_t *name, boo_uint_t value);

#endif //_SYMTAB_
