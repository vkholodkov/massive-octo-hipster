
#include <stdlib.h>
#include <sys/user.h>
#include <sys/types.h>

#include <malloc.h>
#include <string.h>

#include "pool.h"

static size_t pagesize;

void pool_init() {
    pagesize = sysconf(_SC_PAGESIZE);
}

pool_t *pool_create() {
    pool_t *p;
    pchunk_t *c;
    u_char *start;

    p = malloc(pagesize);

    if(p == NULL) {
        perror("malloc");
        exit(255);
    }

    c = (pchunk_t*)(p + 1);

    p->chunks = p->current = c;
    c->next = NULL;
    
    start = (u_char*)p;

    c->allocated = (u_char*)(c + 1);
    c->end = start + pagesize;

    return p;
}

void pool_destroy(pool_t *pool) {
    pchunk_t *c,*f;

    c = pool->chunks;
    c = c->next;

    while(c != NULL) {
        f = c;
        c = c->next;

        free(f);
    }

    free(pool);
}

void *palloc(pool_t *pool, size_t size) {
    pchunk_t *c;
    void *p;
    u_char *start;

    c = pool->chunks;

    while(c != NULL) {
        if(c->end - c->allocated < size) {
            p = c->allocated;
            c->allocated += size;
            return p;
        }

        c = c->next;
    }

    c = malloc(pagesize);

    if(c == NULL) {
        perror("malloc");
        exit(255);
    }

    start = (u_char*)c;

    c->next = NULL;
    c->allocated = start + sizeof(pchunk_t);
    c->end = start + pagesize;

    p = c->allocated;
    c->allocated += size;

    pool->current->next = c;
    pool->current = c;

    return p;
}

void *pcalloc(pool_t *pool, size_t size) {
    void *p;

    p = palloc(pool, size);

    bzero(p, size);

    return p;
}

u_char *pstrdup(pool_t *pool, u_char *str, size_t size) {
    u_char *p;

    p = palloc(pool, size);
    memcpy(p, str, size);
    
    return p;
}

