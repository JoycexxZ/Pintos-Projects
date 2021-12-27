#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (struct dir *dir, const char *name, off_t initial_size, enum entry_type type) 
{
  block_sector_t inode_sector = 0;
  if(strlen(name) == 1 && *name == '.')
    return false;
  if(strlen(name) == 2 && *name == '.' && name[1] == '.')
    return false;
  // struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector, type));
  // dir_close (dir);
  if (type == DIRECTORY)
  {
    struct dir *new_dir = dir_open(inode_open(inode_sector));
    bool success_1 = (new_dir != NULL
                  && dir_add (new_dir, ".", inode_sector, type));
    bool success_2 = (new_dir != NULL
                  && dir_add (new_dir, "..", dir->inode->sector, type));
    dir_close(new_dir);
    success = success_1 && success_2;
  }
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (struct dir *dir, const char *name, enum entry_type *type)
{
  // struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode, type);
  // dir_close (dir);


  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (struct dir *dir, const char *name, block_sector_t cwd) 
{
  // struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name, cwd);
  // dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
