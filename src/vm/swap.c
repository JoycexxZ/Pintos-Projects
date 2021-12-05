#include "vm/swap.h"
#include "vm/page.h"
#include "bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"


void 
swap_init(void){
    struct block *block = block_get_role (BLOCK_SWAP);
    lock_init (&swap_map_lock);
    swap_map = bitmap_create (block_size (block)/PAGE_SECTOR_NUM);
}

size_t 
swap_to_disk(void *kpage){
    lock_acquire (&swap_map_lock);
    size_t index = bitmap_scan_and_flip (swap_map, 0, 1, 0);
    if (index == BITMAP_ERROR){
        lock_release (&swap_map_lock);
        PANIC ("Disk full!");
    }

    struct block *block = block_get_role (BLOCK_SWAP);
    for (int i = index*PAGE_SECTOR_NUM; i < index*PAGE_SECTOR_NUM + PAGE_SECTOR_NUM; i++){
        block_write (block, i, kpage);
        kpage += BLOCK_SECTOR_SIZE;
    }

    lock_release (&swap_map_lock);
    return index;
}

void 
swap_from_disk(size_t index, void *kaddr){
    lock_acquire (&swap_map_lock);
    
    struct block *block = block_get_role (BLOCK_SWAP);

    for (int i = index*PAGE_SECTOR_NUM; i < index*PAGE_SECTOR_NUM + PAGE_SECTOR_NUM; i++){
        if (kaddr == NULL)
            continue;
        block_read (block, i, kaddr);
        kaddr += BLOCK_SECTOR_SIZE;
    }

    bitmap_set(swap_map, index, 0);
    lock_release (&swap_map_lock);
}
