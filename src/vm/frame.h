#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"


struct list frame_table;
struct lock frame_table_lock;



struct frame_table_entry {
    uint32_t* frame;
    struct sup_page_table_entry* vpage;

    bool swap_able;
    bool last_used;

    struct list_elem elem;
};

void frame_table_init();
struct frame_table_entry *frame_get_page(enum palloc_flags flag, struct sup_page_table_entry* page);
void frame_free_page(void* page);



#endif /* vm/frame.h */