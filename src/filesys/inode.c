#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include <stdio.h>
#include <bitmap.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

static size_t
byte_to_dindirect_index (off_t pos){
  size_t sector_id = pos / BLOCK_SECTOR_SIZE;
  return sector_id / BLOCK_IN_INDIRECT;
}

static size_t
byte_to_indirect_index (off_t pos){
  size_t sector_id = pos / BLOCK_SECTOR_SIZE;
  return sector_id - byte_to_dindirect_index (pos) * BLOCK_IN_INDIRECT;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    block_sector_t dindirect_block[BLOCK_IN_INDIRECT];
    block_sector_t indirect_block[BLOCK_IN_INDIRECT];
    // block_read (fs_device, inode->data.dindirect_block, dindirect_block);
    // block_read (fs_device, dindirect_block[byte_to_dindirect_index(pos)], indirect_block);
    cache_block_read (inode->data.dindirect_block, dindirect_block);
    cache_block_read (dindirect_block[byte_to_dindirect_index(pos)], indirect_block);
    return indirect_block[byte_to_indirect_index(pos)];
  }
  else
    return -1;
}

static block_sector_t
allocate_sector_id ()
{
  block_sector_t sector_id;
  if (!free_map_allocate(1, &sector_id))
    return -1;
  static char zeros[BLOCK_SECTOR_SIZE];
  // block_write (fs_device, sector_id, zeros);
  cache_block_write (sector_id, zeros);
  return sector_id;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

static void
inode_disk_clear (struct inode_disk *disk_inode)
{
  if (disk_inode->capacity == 0)
    return;
  size_t dindirect_end = byte_to_dindirect_index (disk_inode->capacity - 1) + 1;
  block_sector_t dindirect_block[BLOCK_IN_INDIRECT];
  block_sector_t indirect_block[BLOCK_IN_INDIRECT];
  // block_read (fs_device, disk_inode->dindirect_block, dindirect_block);
  cache_block_read (disk_inode->dindirect_block, dindirect_block);
  for (size_t i = 0; i < dindirect_end; i++){
    // block_read (fs_device, dindirect_block[i], indirect_block);
    cache_block_read (dindirect_block[i], indirect_block);
    size_t indirect_end = (i == dindirect_end - 1)? 
           byte_to_indirect_index (disk_inode->capacity-1)+1:BLOCK_IN_INDIRECT;
    for (size_t j = 0; j < indirect_end; j++){
      free_map_release (indirect_block[j], 1);
      // printf("%d ", indirect_block[j]);
    }
    // printf("\n");
    free_map_release (dindirect_block[i], 1);
    // printf("i: %d ", dindirect_block[i]);
    // printf("\n");
  }
  free_map_release (disk_inode->dindirect_block, 1);
  // printf("doubly: %d \n", disk_inode->dindirect_block);
}

static bool
inode_disk_growth (struct inode_disk *disk_inode, off_t new_length, bool free_all)
{
  bool success = false;
  if (disk_inode->capacity >= new_length){
    disk_inode->length = new_length;
    success = true;
    goto growth_end;
  }
  off_t ori_length = disk_inode->length;
  size_t ori_sectors = bytes_to_sectors (ori_length);
  size_t new_sectors = bytes_to_sectors (new_length);

  block_sector_t dindirect_block[BLOCK_IN_INDIRECT];
  block_sector_t indirect_block[BLOCK_IN_INDIRECT];

  if (ori_length > 0){
    /* Adjust dindirect and indirect blocks to the end. */
    // block_read (fs_device, disk_inode->dindirect_block, dindirect_block); 
    cache_block_read (disk_inode->dindirect_block, dindirect_block);
    size_t dindirect_end = byte_to_dindirect_index (ori_length - 1);
    // printf ("os: %d, ns: %d, c: %d, index: %d, s1: %d, s2: %d\n", ori_sectors, new_sectors, disk_inode->capacity, dindirect_end, disk_inode->dindirect_block, dindirect_block[dindirect_end]);
    // printf ("1 - 0, 0 sector: %d, 0 sector: %d\n", dindirect_block[0], disk_inode->dindirect_block);
    // block_read (fs_device, dindirect_block[dindirect_end], indirect_block);
    cache_block_read (dindirect_block[dindirect_end], indirect_block);
  }
  else if(new_length > 0){
    /* Allocate a double indirect block first. */
    block_sector_t id = allocate_sector_id ();
    // printf("id1: %d\n", id);
    disk_inode->dindirect_block = id;
  }

  /* New data sectors. */
  for (size_t i = ori_sectors; i < new_sectors; i++){
    size_t dindirect_index = byte_to_dindirect_index (i*BLOCK_SECTOR_SIZE);
    size_t indirect_index = byte_to_indirect_index (i*BLOCK_SECTOR_SIZE);
    // printf ("i: %d, s1: %d, s2: %d\n", i, dindirect_index, indirect_index);

    /* If a new indirect block is needed. */
    if (indirect_index == 0){
      block_sector_t id = allocate_sector_id ();
      // printf("id2: %d\n", id);
      if (id == -1)
        goto growth_end;
      dindirect_block[dindirect_index] = id;
    }

    /* Allocate the new block sector. */
    block_sector_t id = allocate_sector_id ();
    // printf("id3: %d\n", id);
    // printf("ori_length: %d, new_length: %d\n", ori_length, new_length);
    if (id == -1)
      goto growth_end;
    indirect_block[indirect_index] = id;
    disk_inode->capacity += BLOCK_SECTOR_SIZE;
    disk_inode->length = (disk_inode->capacity < new_length)? 
                          disk_inode->capacity : new_length;

    /* If a indirect block is full, save it. */
    if (indirect_index == BLOCK_IN_INDIRECT - 1 || i == new_sectors - 1){
      // block_write (fs_device, dindirect_block[dindirect_index], indirect_block);
      cache_block_write (dindirect_block[dindirect_index], indirect_block);
    }
  }

  if (new_length > 0)
    // block_write(fs_device, disk_inode->dindirect_block, dindirect_block);
    cache_block_write (disk_inode->dindirect_block, dindirect_block);
  success = true;

growth_end:
  if (!success && free_all){
    inode_disk_clear (disk_inode);
  }
  // if (new_length > 0){
  //   block_read (fs_device, disk_inode->dindirect_block, dindirect_block); 
    
  //   printf ("2 - 0, 0 sector: %d, 0 sector: %d\n", dindirect_block[0], disk_inode->dindirect_block);
  // }
  return success;
}


/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);
  
  // printf ("Length: %d\n", length);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  success = inode_disk_growth (disk_inode, length, true);
  // block_write (fs_device, sector, disk_inode);
  cache_block_write (sector, disk_inode);

  free (disk_inode);

 
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  // block_read (fs_device, inode->sector, &inode->data);
  cache_block_read (inode->sector, &inode->data);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  cache_block_write (inode->sector, &inode->data);
  // block_write (fs_device, inode->sector, &inode->data);

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          cache_to_disk ();
          free_map_release (inode->sector, 1);
          // if (flag){
          //   flag--;
          // printf("in inode_close\n");
          // }
          inode_disk_clear (&inode->data);
            // bitmap_dump(free_map);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  // printf ("size: %d, offset: %d, inode length: %d\n", size, offset, inode->data.length);

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          // block_read (fs_device, sector_idx, buffer + bytes_read);
          cache_block_read (sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          // block_read (fs_device, sector_idx, bounce);
          cache_block_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (size + offset > inode->data.length){
    inode_disk_growth (&inode->data, size + offset, false);
  } 


  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      
      // printf("Size: %d, Offset: %d, Sector: %d\n", size, offset, sector_idx); 
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      // printf("offset: %d, capacity: %d, sector id: %d\n", offset, inode->data.capacity, sector_idx);

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      // printf("inde_len: %d, sector_left: %d\n", inode_length(inode), sector_left);
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          // block_write (fs_device, sector_idx, buffer + bytes_written);
          cache_block_write (sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            // block_read (fs_device, sector_idx, bounce);
            cache_block_read (sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // block_write (fs_device, sector_idx, bounce);
          cache_block_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
      // printf("byte_write: %d, chunk_size: %d\n", bytes_written, chunk_size);
    }
  free (bounce);

  // if (inode->data.dindirect_block == 2){
  //   block_sector_t dindirect_block[BLOCK_IN_INDIRECT];
  //   block_read (fs_device, inode->data.dindirect_block, dindirect_block); 
  //   printf ("3 - 0, 0 sector: %d, 0 sector: %d\n", dindirect_block[0], inode->data.dindirect_block);
  // }

  // printf ("bytes written: %d, file size: %d\n", bytes_written, inode->data.length);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
