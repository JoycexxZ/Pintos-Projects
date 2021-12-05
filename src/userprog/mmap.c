#include "userprog/mmap.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

struct lock mmap_lock;
int mmap_id = 0;

void
mmap_init()
{
    lock_init(&mmap_lock);
}

int 
mmap (int fd, void *vadd)
{
    if (vadd == 0) return -1;
    if (fd < 2) return -1;
    int file_size = filesize(fd);
    if (file_size == 0) return -1;
    if (file_size == -1) return -1;
    if (pg_ofs(vadd) != 0) return -1;

    for (size_t i = 0; i < file_size; i+=PGSIZE)
    {
        struct sup_page_table_entry *entry = sup_page_table_look_up (thread_current()->sup_page_table, vadd + i*PGSIZE);
        if (entry) return -1;
    }
    
    lock_acquire(&mmap_lock);
    void *map_start = vadd;
    fd = reopen(fd);
    int page_count = 0;

    for (size_t i = 0; i < file_size; i+=PGSIZE)
    {
        struct sup_page_table_entry *entry = sup_page_create(vadd, PAL_ZERO | PAL_USER, true);
        int file_end = 0;
        if ((i + PGSIZE) < file_size)
            file_end = i + PGSIZE;
        else
            file_end = file_size;

        sup_page_set_file(entry, fd, i, file_end);
        vadd += PGSIZE;
        page_count ++;
    }

    struct mmap_file *mmapfile = (struct mmap_file *)malloc(sizeof (struct mmap_file));
    mmapfile->id = mmap_id++;
    mmapfile->vadd = map_start;
    mmapfile->fd = fd;
    mmapfile->page_count = page_count;

    list_push_back(&thread_current()->mmap_files, &mmapfile->elem);
    lock_release(&mmap_lock);

    return mmapfile->id;
}

void
mmap_discard (struct sup_page_table_entry *entry)
{
    void * pd = entry->owner->pagedir;
    if (pagedir_is_dirty(pd, entry->vadd))
    {
        pagedir_set_dirty(pd, entry->vadd, false);
        page_set_swap_able(entry, false);
        seek(entry->fd, entry->file_start);
        write(entry->fd, entry->vadd,  entry->file_end - entry->file_start);
        page_set_swap_able(entry, true);
    }
}

void 
munmap (int map_id)
{
    lock_acquire(&mmap_lock);
    struct mmap_file *target_map;
    for (struct list_elem * i = list_begin(&thread_current()->mmap_files); i != list_end(&thread_current()->mmap_files); i = list_next(i))
    {
        struct mmap_file *temp = list_entry(i, struct mmap_file, elem);
        if (temp->id == map_id)
        {
            target_map = temp;
        }
    }

    void *vadd  = target_map->vadd;
    for (size_t i = 0; i < target_map->page_count; i++)
    {
        struct sup_page_table_entry *entry = sup_page_table_look_up(thread_current()->sup_page_table, vadd);
        ASSERT(entry);
        mmap_discard(entry);
        page_destroy_by_elem(thread_current()->sup_page_table, entry);
        vadd += PGSIZE;
    }
    close(target_map->fd);
    list_remove(&target_map->elem);
    free(target_map);
    lock_release(&mmap_lock);
}
