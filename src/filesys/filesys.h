#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
extern struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (struct dir *dir, const char *name, off_t initial_size, enum entry_type type);
struct file *filesys_open (struct dir *dir, const char *name, enum entry_type *type);
bool filesys_remove (struct dir *dir, const char *name);

#endif /* filesys/filesys.h */
