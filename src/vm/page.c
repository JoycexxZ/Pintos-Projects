#include "vm/page.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"


void 
sup_page_create (void *upage, enum palloc_flags flag)
{
    struct sup_page_table *page_table = &thread_current ()->sup_page_table;
    lock_acquire(&page_table->table_lock);

    struct sup_page_table_entry* entry = sup_page_table_look_up (page_table, upage);
    if (entry != NULL){
        PANIC("Page entry already exists. ");
    }
    
    entry = (struct sup_page_table_entry*) malloc (sizeof(struct sup_page_table_entry));
    entry->owner = thread_current ();
    entry->vadd = upage;
    entry->flag = flag;
    lock_init(&entry->page_lock);
    list_push_back(page_table, entry);

    lock_release(&page_table->table_lock);
}

void 
sup_page_destroy (struct sup_page_table *table, void *vadd)
{
    struct sup_page_table_entry* entry = sup_page_table_look_up (table, vadd);
    ASSERT (entry != NULL);

    lock_acquire (&table->table_lock);
    list_remove (&entry->elem);
    lock_release (&table->table_lock);

    if (entry->frame != NULL){
        frame_free_page (entry->frame);
        pagedir_clear_page (entry->owner->pagedir, vadd);
    }

    free(entry);
}

bool 
sup_page_activate (struct sup_page_table_entry *entry)
{
    lock_acquire (&entry->page_lock);
    
    lock_release (&entry->page_lock);
}


