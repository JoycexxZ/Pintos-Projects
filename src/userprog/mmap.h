#ifndef SYS_MMAP_H
#define SYS_MMAP_H

#include "threads/thread.h"

/* Necessary info for a mmap */
struct mmap_file
{
    void* vadd;                     /* The starting address of this mapping */
    int id;                         /* The id of this mapping which is unique */
    int page_count;                 /* The numbers of pages of this file hold */
    int fd;                         /* The fd of the mapping file */
    struct list_elem elem;          /* List elem */
};

int mmap (int fd, void *vadd);
void munmap (int map_id);
void mmap_discard (struct sup_page_table_entry *entry);
void mmap_init();

#endif