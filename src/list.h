
#ifndef _BOO_LIST_H_
#define _BOO_LIST_H_

#include <sys/types.h>

typedef struct boo_list_entry_s {
    struct boo_list_entry_s     *next, *prev;
} boo_list_entry_t;

typedef struct {
    size_t                  entry_size;
    boo_list_entry_t        entries;
} boo_list_t;

#define boo_list_empty(list) ((list)->entries.next == &(list)->entries)
#define boo_list_begin(list) (void*)(list)->entries.next
#define boo_list_end(list) (void*)&((list)->entries)
#define boo_list_next(p) (void*)(p)->entry.next

#define boo_list_append(list, entry) boo_list_insert((list), (entry), &(list)->entries);

#define boo_list_remove(entry) \
    do { \
        (entry)->next->prev = (entry)->prev; \
        (entry)->prev->next = (entry)->next; \
    } while(0);

#define boo_list_splice(to, from) \
        if((from)->entries.next != &(from)->entries) { \
            (from)->entries.next->prev = (to)->entries.prev; \
            (to)->entries.prev->next = (from)->entries.next; \
            (from)->entries.prev->next = &(to)->entries; \
            (to)->entries.prev = (from)->entries.prev; \
            (from)->entries.next = &(from)->entries; \
            (from)->entries.prev = &(from)->entries; \
        }

void boo_list_init(boo_list_t*);
void boo_list_insert(boo_list_t*, boo_list_entry_t*, boo_list_entry_t*);

#endif
