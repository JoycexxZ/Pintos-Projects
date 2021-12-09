#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"

/* Frame table that stores all frames globally.  */
struct list frame_table;

/* Lock of frame table. */
struct lock frame_table_lock;

int evict_num;

int frame_num;

/* Pointer of next frame to evict. */
struct list_elem* frame_table_evict_ptr;

/* Frame table entry in frame_table. */
struct frame_table_entry {
    uint32_t* frame;                            /* Kernel virtual address of this frame. */
    struct sup_page_table_entry* vpage;         /* The corresponding page entry. */

    bool swap_able;                             /* Able to swap. */
    bool last_used;                             /* Last used bit in second chance algorithm. */

    struct list_elem elem;                      /* List elem in frame_table. */
};

void frame_table_init();
struct frame_table_entry *frame_get_page(enum palloc_flags flag, struct sup_page_table_entry* page);
void frame_free_page(void* page);
void evict_frame();



#endif /* vm/frame.h */