#include <string.h>
#include <stdio.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"


static size_t 
cache_table_lookup (block_sector_t sector)
{
    for (size_t i = 0; i < CACHE_SIZE; i++){
        if (cache_table[i].valid && cache_table[i].sector == sector)
            return i;
    }
    return -1;
}

static void
cache_entry_writeback (struct cache_table_entry *entry)
{
    if (entry->valid && entry->dirty){
        block_write (fs_device, entry->sector, entry->data);
        entry->dirty = false;
    }
}

static size_t
cache_table_find_empty ()
{
    for (size_t i = 0; i < CACHE_SIZE; i++){
        if (!cache_table[i].valid)
            return i;
    }
    for (;;cache_table_evict_id++){
        if (cache_table_evict_id >= CACHE_SIZE)
            cache_table_evict_id = 0;
        struct cache_table_entry *e = &cache_table[cache_table_evict_id];
        if (e->valid && !e->last_used){
            cache_entry_writeback (e);
            return cache_table_evict_id;
        }
        else{
            e->last_used = !e->last_used;
        }
    }
}

static void
save_to_cache (block_sector_t sector, void *data, bool dirty)
{
    // printf ("used: %d\n", cache_table_used);
    struct cache_table_entry *e;
    if (cache_table_used < CACHE_SIZE){
        e = &cache_table[cache_table_used];
        cache_table_used ++;
    }
    else {
        e = &cache_table[cache_table_find_empty ()];
    }
    memcpy (e->data, data, sizeof e->data);
    e->sector = sector;
    e->dirty = dirty;
    e->valid = true;
    e->last_used = true;
}

void 
cache_table_init ()
{
    lock_init (&cache_table_lock);
    memset (cache_table, 0, sizeof cache_table);
    cache_table_evict_id = 0;
    cache_table_used = 0;
}

void 
cache_block_read (block_sector_t sector, void *buffer)
{
    lock_acquire (&cache_table_lock);
    // printf ("Read\n");
    size_t entry_id = cache_table_lookup (sector);
    if (entry_id != -1){
        struct cache_table_entry *entry;
        entry = &cache_table[entry_id];
        memcpy (buffer, entry->data, sizeof entry->data);
        entry->last_used = true;
    }
    else {
        block_read (fs_device, sector, buffer);
        save_to_cache (sector, buffer, false);
    }
    lock_release (&cache_table_lock);
}


void
cache_block_write (block_sector_t sector, void *buffer)
{
    lock_acquire (&cache_table_lock);
    // printf ("Write\n");
    size_t entry_id = cache_table_lookup (sector);
    if (entry_id != -1){
        struct cache_table_entry *entry;
        entry = &cache_table[entry_id];
        memcpy (entry->data, buffer, sizeof entry->data);
        entry->last_used = true;
        entry->dirty = true;
    }
    else{
        save_to_cache (sector, buffer, true);
    }
    lock_release (&cache_table_lock);
}

void 
cache_to_disk ()
{
    lock_acquire (&cache_table_lock);
    for (size_t i = 0; i < CACHE_SIZE; i++){
        struct cache_table_entry* entry = &cache_table[i];
        cache_entry_writeback (entry);
    }
    lock_release (&cache_table_lock);
}