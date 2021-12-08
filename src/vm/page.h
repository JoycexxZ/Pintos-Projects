#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "devices/block.h"

/* Page entry status number. */
enum sup_page_table_entry_status{
    FRAME,
    SWAP,
    EMPTY
};

/* Supplemental page table entry. */
struct sup_page_table_entry {
    struct thread* owner;                       /* Owner thread of this page. */ 
    void* vadd;                                 /* Virtual address of this page. */
    enum palloc_flags flag;                     /* Palloc flag when we want to create a frame 
                                                   that associate with this page. */

    union entry_data                            
    {
        struct frame_table_entry * frame;       /* The associated frame entry under FRAME status. */
        size_t swap_index;                      /* The corresponding index on swap table under
                                                   SWAP status. */
    }value;                                     /* Data according to different status. */

    enum sup_page_table_entry_status status;    /* Status of this page. */

    bool writable;                              /* Writable bit. */
    struct lock page_lock;                      /* Lock of this page. */

    int fd;                                     /* File descriptor if this page associates with a file. */
    int file_start;                             /* The start position of file that stored in this page. */
    int file_end;                               /* The end position of file. */
    struct list_elem elem;                      /* List elem for `sup_page_table` */
};

/* Supplemental page table. */
struct sup_page_table
{
    struct list table;              /* List that stores all entries. */
    struct lock table_lock;         /* Lock of this table */
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