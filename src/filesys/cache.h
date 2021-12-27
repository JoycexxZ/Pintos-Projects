#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

struct cache_table_entry
{
    block_sector_t sector;

    uint8_t data[BLOCK_SECTOR_SIZE];

    bool dirty;
    bool valid;
    bool last_used;
    struct lock lock;
};

struct cache_table_entry cache_table[CACHE_SIZE];



#endif /* filesys/cache.h */
