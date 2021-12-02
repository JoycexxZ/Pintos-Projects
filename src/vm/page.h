#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"


struct sup_page_table_entry {
    struct thread* owner;
    
    
    uint32_t* vadd;

    struct lock page_lock;
    struct list_elem elem;
};

struct sup_page_table
{
    struct list table;
    struct lock table_lock;
};


struct sup_page_table_entry sup_page_table_look_up (struct sup_page_table table, void *vaddr);
struct sup_page_table * sup_page_table_init();
void sup_page_table_destroy(struct sup_page_table *table);
void sup_page_create (void *upage, enum palloc_flags flag);
void sup_page_destroy(struct sup_page_table *table, void *vadd);
bool sup_page_activate(struct sup_page_table_entry *entry);

#endif /* vm/page.h */