
#ifndef _POOL_
#define _POOL_

#include <sys/types.h>

typedef struct pchunk {
    struct pchunk *next;
    u_char *allocated, *end;
} pchunk_t;

typedef struct {
    pchunk_t *chunks, *current;
} pool_t;

void pool_init();
pool_t *pool_create();
void pool_destroy(pool_t*);

void *palloc(pool_t*, size_t);
void *pcalloc(pool_t*, size_t);
u_char *pstrdup(pool_t*, u_char*, size_t);

#endif //_POOL_
