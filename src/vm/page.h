#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "devices/block.h"

enum sup_page_table_entry_status{
    FRAME,
    SWAP,
    EMPTY
};

struct sup_page_table_entry {
    struct thread* owner;    
    void* vadd;
    enum palloc_flags flag;

    union entry_data
    {
        struct frame_table_entry * frame;
        size_t swap_index;
    }value;

    enum sup_page_table_entry_status status;

    bool writable;
    struct lock page_lock;

    int fd;
    int file_start;
    int file_end;
    struct list_elem elem;
};

struct sup_page_table
{
    struct list table;
    struct lock table_lock;
};

struct sup_page_table_entry *sup_page_table_look_up (struct sup_page_table *table, void *vaddr);
struct sup_page_table *sup_page_table_init ();
void sup_page_table_destroy (struct sup_page_table *table);
struct sup_page_table_entry *sup_page_create (void *upage, enum palloc_flags flag, bool writable);
void sup_page_destroy (struct sup_page_table *table, void *vadd);
bool sup_page_activate (struct sup_page_table_entry *entry);
void page_destroy_by_elem (struct sup_page_table *table, struct sup_page_table_entry *entry);
void page_set_swap_able(struct sup_page_table_entry *entry, bool swap_able);
void sup_page_set_file (struct sup_page_table_entry *entry, int fd, int file_start, int file_end);

#endif /* vm/page.h */