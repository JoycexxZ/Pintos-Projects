#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/off_t.h"

/* Cache entry number in cache table. */
#define CACHE_SIZE 64

/* Cache table entry. */
struct cache_table_entry
{
    block_sector_t sector;              /* The sector of the data store */
    uint8_t data[BLOCK_SECTOR_SIZE];    /* The data in memory */
    bool dirty;                         /* Dirty flag */
    bool valid;                         /* Valid falg */
    bool last_used;                     /* Last used flag */
};

/* Cache table of CACHE_SIZE entries. */
struct cache_table_entry cache_table[CACHE_SIZE];

/* Lock for cache table. */
struct lock cache_table_lock;

/* Next id to try evicting in cache table. */
size_t cache_table_evict_id;

/* Used length of cache table */
size_t cache_table_used;

void cache_table_init ();
void cache_block_read (block_sector_t sector, void *buffer);
void cache_block_write (block_sector_t sector, void *buffer);
void cache_to_disk ();


#endif /* filesys/cache.h */
