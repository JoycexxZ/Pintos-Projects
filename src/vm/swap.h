#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

/* Sector number to store a single page. */
#define PAGE_SECTOR_NUM (PGSIZE/BLOCK_SECTOR_SIZE)

/* Bitmap to track the usage of swap disk. */
struct bitmap* swap_map;
/* Lock of swap_map. */
struct lock swap_map_lock;

void swap_init(void);
size_t swap_to_disk(void *kpage);
void swap_from_disk(size_t index, void *kaddr);

#endif /* vm/swap.h */