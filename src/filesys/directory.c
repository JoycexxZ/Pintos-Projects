#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"


static bool
dir_is_empty (struct dir *dir){
  struct dir_entry e;
  off_t pos = 0;

  while (inode_read_at (dir->inode, &e, sizeof e, pos) == sizeof e) 
  {
    pos += sizeof e;
    if (e.in_use && e.name != ".." && e.name != ".")
      return true;
  }
  return false;
}

static void
dir_clear (struct dir *dir){
  struct dir_entry e;
  off_t pos = 0;

  while (inode_read_at (dir->inode, &e, sizeof e, pos) == sizeof e) 
  {
    pos += sizeof e;
    if (e.in_use){
      struct inode *inode = inode_open (e.inode_sector);
      inode_remove (inode);
      inode_close (inode);
    }
  }
}

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  //struct lock locallock;
  //lock_init(&locallock);
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  //lock_acquire(&locallock);
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        //lock_release(&locallock);
        return true;
      }
  //lock_release(&locallock);
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode, enum entry_type *type) 
{
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  struct dir_entry e;
  char name_copy[100];
  strlcpy (name_copy, name, strlen(name)+1);
  char *token, *save_ptr, *last;
  struct dir *cur_dir;
  enum entry_type cur_type;
  struct inode *cur_inode;
  *inode = NULL;

  if (lookup (dir, name, &e, NULL)){
    *inode = inode_open (e.inode_sector);
    *type = e.type;
    goto done;
  }

  if (strlen(name) == 0)
  {
    *inode = dir->inode;
    goto done;
  }

  if (name[0] == '/'){
    // printf("hh\n");
    cur_dir = dir_open_root ();
    cur_inode = cur_dir->inode; 
  }
  else{
    cur_dir = dir_reopen(dir);
    cur_inode = cur_dir->inode; 
  }
  cur_type = DIRECTORY;

  for (token = strtok_r (name_copy, "/", &save_ptr);
       token != NULL;
       token = strtok_r (NULL, "/", &save_ptr)){
    printf ("in loop, token: %s\n", token);
    if (cur_type != DIRECTORY)
      goto done;
    if (!lookup (cur_dir, token, &e, NULL))
      goto done;
    cur_type = e.type;
    cur_inode = inode_open (e.inode_sector);
    dir_close (cur_dir);
    if (cur_type == DIRECTORY){
      cur_dir = dir_open (cur_inode);
    }
  }
  *inode = cur_inode;  
  *type = cur_type;

  
done:
  printf ("in look up, name: %s, inode: %x\n", name, *inode);

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, enum entry_type type)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  e.type = type;
  e.parent = dir;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name, block_sector_t cwd) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  if (e.inode_sector == 1 || e.inode_sector == cwd)
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  if (e.type == DIRECTORY){
    if (inode->open_cnt > 1)
      goto done;
    struct dir *deleted_dir = dir_open (inode);
    if (!dir_is_empty (deleted_dir)){
      free (deleted_dir);
      goto done;
    }
    dir_clear (deleted_dir);
    free (deleted_dir);
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use && strcmp(e.name, "..") && strcmp(e.name, "."))
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}


struct dir *get_parent_dir (struct dir_entry *e){
  return e->parent;
}