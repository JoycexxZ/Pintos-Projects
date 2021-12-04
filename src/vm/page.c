#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

struct sup_page_table_entry *
sup_page_table_look_up (struct sup_page_table *table, void *vaddr)
{
    if (vaddr == NULL)
        return NULL;
    
    for (struct list_elem *i = list_begin(&table->table); i != list_end(&table->table); i = list_next(i))
    {
        struct sup_page_table_entry *temp = list_entry(i, struct sup_page_table_entry, elem);
        if ((uint32_t)(temp->vadd) >> 12 == (uint32_t)vaddr >> 12)
        {
            if (temp->status == FRAME){
                temp->value.frame->last_used = true;
            }
            return temp;
        }
    }
    return NULL;
}

struct sup_page_table * 
sup_page_table_init()
{
    struct sup_page_table *table = (struct sup_page_table *)malloc(sizeof(struct sup_page_table));
    list_init(&table->table);
    lock_init(&table->table_lock);
    return table;
}

void 
sup_page_table_destroy(struct sup_page_table *table)
{
    for (struct list_elem *i = list_begin(&table->table); i != list_end(&table->table);)
    {
        struct list_elem *next_i = list_next(i);
        struct sup_page_table_entry *temp = list_entry(i, struct sup_page_table_entry, elem);
        page_destroy_by_elem(table, temp);
        i = next_i;
    }
    free(table);
}

struct sup_page_table_entry *
sup_page_create (void *upage, enum palloc_flags flag, bool writable)
{

    ASSERT(is_user_vaddr(upage));

    struct sup_page_table *page_table = thread_current ()->sup_page_table;
    lock_acquire(&page_table->table_lock);

    struct sup_page_table_entry* entry = sup_page_table_look_up (page_table, upage);
    if (entry != NULL){
        PANIC("Page entry already exists. ");
    }
    
    entry = (struct sup_page_table_entry*) malloc (sizeof(struct sup_page_table_entry));
    lock_init(&entry->page_lock);
    lock_acquire (&entry->page_lock);
    entry->owner = thread_current ();
    entry->vadd = upage;
    entry->flag = flag;
    entry->writable = writable;
    entry->status = EMPTY;
    list_push_back(page_table, &entry->elem);

    lock_release(&entry->page_lock);
    lock_release(&page_table->table_lock);
    ASSERT(entry!=NULL);
    return entry;
}

void 
sup_page_destroy (struct sup_page_table *table, void *vadd)
{
    struct sup_page_table_entry* entry = sup_page_table_look_up (table, vadd);
    page_destroy_by_elem(table, entry);
}

bool 
sup_page_activate (struct sup_page_table_entry *entry)
{
    ASSERT(is_user_vaddr(entry->vadd));
    ASSERT(entry->owner->pagedir != NULL);
    lock_acquire (&entry->page_lock);
    if (pagedir_get_page(entry->owner->pagedir, entry->vadd)) {
        lock_release (&entry->page_lock);
        return false;
    }

    struct frame_table_entry *frame_entry = frame_get_page(entry->flag, entry);
    void* frame = frame_entry->frame;

    ASSERT (vtop (frame) >> PTSHIFT < init_ram_pages);
    ASSERT (pg_ofs (frame) == 0);
    if(!pagedir_set_page(entry->owner->pagedir, entry->vadd, frame, entry->writable)){
        lock_release (&entry->page_lock);
        return false;
    }

    if (entry->status == SWAP){
        swap_from_disk (entry->value.swap_index, frame);
    }

    entry->value.frame = frame_entry;
    entry->status = FRAME;

    lock_release (&entry->page_lock);
    return true;
}


void 
page_destroy_by_elem (struct sup_page_table *table, struct sup_page_table_entry *entry)
{
    ASSERT (entry != NULL);

    lock_acquire (&table->table_lock);
    list_remove (&entry->elem);

    if (entry->status == FRAME && entry->value.frame != NULL){
        frame_free_page (entry->value.frame->frame);
        pagedir_clear_page (entry->owner->pagedir, entry->vadd);
    }
    if (entry->status == SWAP){
        void *temp = palloc_get_page(PAL_USER | PAL_ZERO);
    }

    free(entry);
    lock_release (&table->table_lock);
}

void
page_set_swap_able(struct sup_page_table_entry *entry, bool swap_able)
{
    ASSERT(entry != NULL);

    lock_acquire(&entry->page_lock);
    if (entry->status == FRAME && entry->value.frame->frame != NULL)
        entry->value.frame->swap_able = swap_able;
    else{
        sup_page_activate(entry);
    }
    lock_release(&entry->page_lock);
}
