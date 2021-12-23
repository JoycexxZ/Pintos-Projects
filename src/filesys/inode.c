#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCK_NUM 12
#define INDIRECT_BLOCK_NUM 2
#define BLOCK_IN_INDIRECT 128

static bool inode_growth (struct inode *inode, off_t length);

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct_blocks[DIRECT_BLOCK_NUM];
    block_sector_t indirect_block[INDIRECT_BLOCK_NUM];
    off_t length;
    unsigned magic;                     /* Magic number. */
    uint32_t unused[112];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos >= inode->data.length)
    return -1;

  int index = pos / BLOCK_SECTOR_SIZE;
  if (index < DIRECT_BLOCK_NUM)
    return inode->data.direct_blocks[index];

  index -= DIRECT_BLOCK_NUM;
  block_sector_t ind_block_index = inode->data.indirect_block[index / BLOCK_IN_INDIRECT];
  block_sector_t buffer[BLOCK_IN_INDIRECT];
  block_read(fs_device, ind_block_index, buffer);
  return buffer[index % BLOCK_IN_INDIRECT];  
}

static int 
byte_to_indirect_block(off_t pos){
  if (pos - DIRECT_BLOCK_NUM < 0)
    return -1;
  return (pos - DIRECT_BLOCK_NUM) / BLOCK_IN_INDIRECT;
}

static int 
byte_to_indirect_index(off_t pos){
  if (pos - DIRECT_BLOCK_NUM < 0)
    return -1;
  return (pos - DIRECT_BLOCK_NUM) % BLOCK_IN_INDIRECT;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

static void 
initialize_block (block_sector_t sector){
    static char zeros[BLOCK_SECTOR_SIZE];
    block_write (fs_device, sector, zeros);
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

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (!disk_inode)
    return false;

  disk_inode->length = length;
  size_t sector_num = bytes_to_sectors(length);

  /* Initialize all blocks of data. */
  block_sector_t buffer[2*BLOCK_IN_INDIRECT];
  for (size_t i = 0; i < sector_num; i++){
    block_sector_t sector_id;
    if (!free_map_allocate (1, &sector_id)){
      for (size_t j = 0; j < i; j++){
        if (i < DIRECT_BLOCK_NUM){
          free_map_release(disk_inode->direct_blocks[j], 1);
        }
        else {
          free_map_release(buffer[j-DIRECT_BLOCK_NUM], 1);
        }
      }
      free (disk_inode);
      return false;
    }
    initialize_block (sector_id);

    if (i < DIRECT_BLOCK_NUM){
      disk_inode->direct_blocks[i] = sector_id;
    }
    else {
      buffer[i-DIRECT_BLOCK_NUM] = sector_id;
    }
  }

  if (sector_num <= DIRECT_BLOCK_NUM)
    return true;

  /* Initialize indirect block if neccesary. */
  size_t indirect_sector_num = sector_num - DIRECT_BLOCK_NUM;
  int indirect_blocks = DIV_ROUND_UP (indirect_sector_num, BLOCK_IN_INDIRECT);
  for (int i = 0; i < indirect_blocks; i++){
    block_sector_t sector_id;
    if (!free_map_allocate (1, &sector_id)){
      if (i != 0)
        free_map_release (sector_id, 1);
      free (disk_inode);
      return false;
    }
    initialize_block (sector_id);
    block_write (fs_device, sector_id, buffer+i*BLOCK_IN_INDIRECT);
    disk_inode->indirect_block[i] = sector_id;
  }

  return true;
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
  block_read (fs_device, inode->sector, &inode->data);
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

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
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
          block_read (fs_device, sector_idx, buffer + bytes_read);
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
          block_read (fs_device, sector_idx, bounce);
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

  if (size + offset > inode_length (inode))
    inode_growth (inode, size + offset);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
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
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

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


static bool 
inode_growth (struct inode_disk *disk_inode, off_t new_length, bool free_all){
  off_t original_length = disk_inode->length;
  size_t ori_sector_num = bytes_to_sectors (disk_inode->length);
  size_t new_sector_num = bytes_to_sectors (new_length);
  off_t length = 0;

  int ori_ind_block_num = byte_to_indirect_block(original_length) + 1;
  int new_ind_block_num = byte_to_indirect_block(new_length) + 1;
  for (int i = ori_ind_block_num; i < new_ind_block_num; i++){
    block_sector_t sector_id;
    if (!free_map_allocate (1, &sector_id) && free_all){
      if (i != 0)
        free_map_release (sector_id, 1);
      free (disk_inode);
      return false;
    }
    initialize_block (sector_id);
  }

  block_sector_t buffer[2*BLOCK_IN_INDIRECT];
  if (ori_sector_num > DIRECT_BLOCK_NUM){
    size_t indirect_sector_num = ori_sector_num - DIRECT_BLOCK_NUM;
    int indirect_blocks = DIV_ROUND_UP (indirect_sector_num, BLOCK_IN_INDIRECT);
    for (int i = 0; i < indirect_blocks; i++){
      block_read (fs_device, disk_inode->indirect_block[i], buffer+i*BLOCK_IN_INDIRECT);
    }
  }

  /* Initialize all blocks of data. */
  for (size_t i = ori_sector_num; i < new_sector_num; i++){
    block_sector_t sector_id;
    if (!free_map_allocate (1, &sector_id)){
      if (free_all){
        for (size_t j = 0; j < i; j++){
          if (i < DIRECT_BLOCK_NUM){
            free_map_release(disk_inode->direct_blocks[j], 1);
          }
          else {
            free_map_release(buffer[j-DIRECT_BLOCK_NUM], 1);
          }
        }
        free (disk_inode);
        return false;
      }
      else{

      }
    }
    initialize_block (sector_id);
    length += BLOCK_SECTOR_SIZE;

    if (i < DIRECT_BLOCK_NUM){
      disk_inode->direct_blocks[i] = sector_id;
    }
    else {
      buffer[i-DIRECT_BLOCK_NUM] = sector_id;
    }
  }

  if (new_sector_num <= DIRECT_BLOCK_NUM)
    return true;

  /* Initialize indirect block if neccesary. */
  size_t indirect_sector_num = new_sector_num - DIRECT_BLOCK_NUM;
  int indirect_blocks = DIV_ROUND_UP (indirect_sector_num, BLOCK_IN_INDIRECT);
  for (int i = 0; i < indirect_blocks; i++){
    block_sector_t sector_id;
    if (!free_map_allocate (1, &sector_id) && free_all){
      for (int j = 0; j < DIRECT_BLOCK_NUM; j++)
        free_map_release (disk_inode->direct_blocks[j], 1);
      if (i != 0)
        free_map_release (sector_id, 1);
      free (disk_inode);
      return false;
    }
    initialize_block (sector_id);
    block_write (fs_device, sector_id, buffer+i*BLOCK_IN_INDIRECT);
    disk_inode->indirect_block[i] = sector_id;
  }

  return true;
}


