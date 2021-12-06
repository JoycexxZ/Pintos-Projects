#ifndef SYS_MMAP_H
#define SYS_MMAP_H

#include "threads/thread.h"


struct mmap_file
{
    void* vadd;
    int id;
    int page_count;
    int fd;
    struct list_elem elem;
};

int mmap (int fd, void *vadd);
void munmap (int map_id);
void mmap_discard (struct sup_page_table_entry *entry);
void mmap_init();

#endif