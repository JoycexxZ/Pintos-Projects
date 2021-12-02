#include "vm/page.h"

struct sup_page_table_entry *
sup_page_table_look_up (struct sup_page_table *table, void *vaddr)
{
    if (vaddr == NULL)
        return NULL;
    
    for (struct list_elem *i = list_begin(&table->table); i != list_end(&table->table); i = list_next(i))
    {
        struct sup_page_table_entry *temp = list_entry(i, struct sup_page_table_entry, elem);
        if ((uint32_t)(temp->vadd) >> 12 == (uint32_t)vaddr >> 12)
        {
            return temp;
        }
    }
    return NULL;
}

struct sup_page_table * 
sup_page_table_init()
{
    struct sup_page_table *table = (struct sup_page_table *)malloc(sizeof(struct sup_page_table));
    list_init(&table->table);
    lock_init(&table->table_lock);
    return table;
}

void 
sup_page_table_destroy(struct sup_page_table *table)
{
    for (struct list_elem *i = list_begin(&table->table); i != list_end(&table->table); i=list_next(i))
    {
        struct sup_page_table_entry *temp = list_entry(i, struct sup_page_table_entry, elem);
        if (temp == NULL) continue;
        sup_page_destroy(table, temp->vadd);
    }
    free(table);
}