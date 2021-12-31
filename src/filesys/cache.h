#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/off_t.h"

#define CACHE_SIZE 64

struct cache_table_entry
{
    block_sector_t sector;

    uint8_t data[BLOCK_SECTOR_SIZE];

    bool dirty;
    bool valid;
    bool last_used;
};

struct cache_table_entry cache_table[CACHE_SIZE];
struct lock cache_table_lock;
size_t cache_table_evict_id;
size_t cache_table_used;

void cache_table_init ();
void cache_block_read (block_sector_t sector, void *buffer);
void cache_block_write (block_sector_t sector, void *buffer);
void cache_to_disk ();


#endif /* filesys/cache.h */
