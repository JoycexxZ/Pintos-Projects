#include "vm/frame.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"


void
frame_table_init()
{
    list_init(&frame_table);
    lock_init(&frame_table_lock);
<<<<<<< HEAD
    lock_init(&frame_table_evict_ptr_lock);

    evict_num = 0;
=======
>>>>>>> 93a145344f5157f9b018be5dc5879809768197c1
}

struct frame_table_entry *
frame_get_page(enum palloc_flags flag, struct sup_page_table_entry* vpage)
{
    int temp = 2;
    while (temp--)
    {
        void* page = palloc_get_page(flag);

        if (page != NULL)
        {
            struct frame_table_entry* f = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));
            ASSERT (f != NULL);
            f->frame = page;
            f->vpage = vpage;
            f->last_used = 0;
            f->swap_able = 1;

            pagedir_set_accessed(vpage->owner->pagedir, page, false);
            pagedir_set_dirty(vpage->owner->pagedir, page, false);

            lock_acquire(&frame_table_lock);
            list_push_back(&frame_table, &f->elem);
            lock_release(&frame_table_lock);
            
            return f;
        }

        /* evict */
        evict_frame ();
    }
    
    printf("get a NULL!!!!\n");
    return NULL;
}

void
frame_free_page(void* page)
{
    lock_acquire(&frame_table_lock);
    for (struct list_elem* i = list_begin(&frame_table); i != list_end(&frame_table); i = list_next(i))
    {
        struct frame_table_entry *temp = list_entry(i, struct frame_table_entry, elem);
        if (temp->frame == (uint32_t*)page)
        {
            palloc_free_page(page);
            list_remove(&temp->elem);

            free(temp);
            lock_release(&frame_table_lock);
            return;
        } 
    } 
}

void
evict_frame()
{
    evict_num++;
    if (evict_num%128== 0){
        bitmap_dump(swap_map);
    }
    if (frame_table_evict_ptr == NULL)
        frame_table_evict_ptr = list_begin (&frame_table);

    while (true)
    {
        struct frame_table_entry* f = list_entry (frame_table_evict_ptr, struct frame_table_entry, elem);
        if (f->swap_able){
            if (!f->last_used){
                size_t idx = swap_to_disk ((void *)f->frame);
                f->vpage->status = SWAP;
                f->vpage->value.swap_index = idx;
                pagedir_clear_page (f->vpage->owner->pagedir, pg_round_down(f->vpage->vadd));
                frame_table_evict_ptr = list_next (frame_table_evict_ptr);
                if (frame_table_evict_ptr == list_end (&frame_table))
                    frame_table_evict_ptr = list_begin (&frame_table);
                frame_free_page (f->frame);
                break;
            }
            else{
                f->last_used = !f->last_used;
            }
        }
        frame_table_evict_ptr = list_next (frame_table_evict_ptr);
        if (frame_table_evict_ptr == list_end (&frame_table))
            frame_table_evict_ptr = list_begin (&frame_table);
    }
    
}