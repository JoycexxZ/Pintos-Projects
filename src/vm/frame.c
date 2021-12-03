#include "vm/frame.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"


void
frame_table_init()
{
    list_init(&frame_table);
    lock_init(&frame_table_lock);
}

void*
frame_get_page(enum palloc_flags flag, struct sup_page_table_entry* vpage)
{
    void* page = palloc_get_page(flag);

    /* swop */

    if (page != NULL)
    {
        struct frame_table_entry* f = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));
        f->frame = page;
        f->vpage = vpage;

        pagedir_set_accessed(vpage->owner->pagedir, page, false);
        pagedir_set_dirty(vpage->owner->pagedir, page, false);

        lock_acquire(&frame_table_lock);
        list_push_back(&frame_table, &f->elem);
        lock_release(&frame_table_lock);
    }

    return page;
}

void
frame_free_page(void* page)
{
    for (struct list_elem* i = list_begin(&frame_table); i != list_end(&frame_table); i = list_next(i))
    {
        struct frame_table_entry *temp = list_entry(i, struct frame_table_entry, elem);
        if (temp->frame == (uint32_t*)page)
        {
            palloc_free_page(page);
            lock_acquire(&frame_table_lock);
            list_remove(&temp->elem);
            lock_release(&frame_table_lock);
            free(temp);
            return;
        } 
    }
    
}