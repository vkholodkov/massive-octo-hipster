
#include "boo.h"

#include "list.h"

void boo_list_init(boo_list_t *list)
{
    list->entry_size = 1;
    list->entries.next = &list->entries;
    list->entries.prev = &list->entries;
}

void boo_list_insert(boo_list_t *list, boo_list_entry_t *p, boo_list_entry_t *before)
{
    p->prev = before->prev;
    p->next = before;

    before->prev->next = p;
    before->prev = p;
}

void boo_list_insert_after(boo_list_t *list, boo_list_entry_t *p, boo_list_entry_t *after)
{
    p->next = after->next;
    p->prev = after;

    after->next->prev = p;
    after->next = p;
}
