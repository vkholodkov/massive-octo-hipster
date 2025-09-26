
#include "pool.h"

#include "symtab.h"

#define HASH_ENTRIES        64

static u_char symtab_hash(boo_str_t *name);

symtab_t     *global_symtab = 0;

symtab_t *symtab_create(pool_t *pool) {
    symtab_t        *t;

    t = palloc(pool, sizeof(symtab_t));

    if(t == 0) {
        return 0;
    }

    t->hashtab = pcalloc(pool, HASH_ENTRIES * sizeof(symbol_t*));

    if(t == 0) {
        return 0;
    }

    t->pool = pool;

    return t;
}

symbol_t *symtab_resolve(symtab_t *symtab, boo_str_t *name) {
    u_char hash;
    symbol_t *p;
    
    hash = symtab_hash(name) % HASH_ENTRIES;

    p = symtab->hashtab[hash];

    while(p) {
        if(!boo_strcmp(&p->name, name))
            return p;

        p = p->next;
    }

    return 0;
}

symbol_t* symtab_add(symtab_t *symtab, boo_str_t *name, boo_uint_t value) {
    u_char hash;
    symbol_t *p, *n;
    
    hash = symtab_hash(name) % HASH_ENTRIES;

    p = symtab->hashtab[hash];

    while(p) {
        if(!boo_strcmp(&p->name, name))
            return 0;

        p = p->next;
    }

    n = pcalloc(symtab->pool, sizeof(symbol_t));

    if(n == 0) {
        return 0;
    }

    n->value = value;
    n->name = *name;
    n->next = symtab->hashtab[hash];

    symtab->hashtab[hash] = n;

    return n;
}

static u_char symtab_hash(boo_str_t *name) {
    u_char          hash, *p;
    size_t          c;

    p = name->data;
    c = name->len;
    hash = 0;
    
    while(c--) {
        hash ^= *p;
        p++;
    }

    return hash;
}
