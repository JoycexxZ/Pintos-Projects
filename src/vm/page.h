#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"


struct sup_page_table_entry {
    uint32_t** pagedir;
    
    uint32_t* vadd;

    
};


#endif /* vm/page.h */