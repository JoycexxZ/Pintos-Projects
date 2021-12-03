#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

struct bitmap swap_map;
struct lock swap_map_lock;

void swap_init(void);
void swap_from_disk(block_sector_t t, void *vaddr_p);
block_sector_t swap_to_disk(void *kpage);

#endif /* vm/swap.h */