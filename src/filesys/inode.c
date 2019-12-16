#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define MAX_SECTOR_SIZE 128 * 128 + 128 + 123

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    bool dir;                           /* Is directory flag */

    block_sector_t direct[123];
    block_sector_t s_indirect;
    block_sector_t d_indirect;
  };

struct indirect
  {
    block_sector_t blocks[128];
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
    struct lock inode_lock;
  };

static struct indirect *read_indirect (block_sector_t sector);
static void inode_allocate(struct inode_disk *, size_t, size_t);

static struct indirect *read_indirect (block_sector_t sector) {
  struct indirect *block = calloc (1, sizeof (struct indirect));
  cache_read (fs_device, sector, block, BLOCK_SECTOR_SIZE, 0);
  return block;
}

static block_sector_t
new_byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  int sectors = pos / BLOCK_SECTOR_SIZE;

  if (0 > pos || pos >= inode->data.length) {
    return -1;
  }

  if (sectors < 123)
  {
    return inode->data.direct[sectors];
  }
  else if (sectors < 123 + 128)
  {
    block_sector_t s_indirect = inode->data.s_indirect;
    struct indirect *block = read_indirect (s_indirect);
    block_sector_t sector = block->blocks[sectors - 123];
    free (block);
    return sector;
  }
  else if (sectors < 123 + 128 + 128 * 128)
  {
    block_sector_t d_indirect_1 = inode->data.d_indirect;
    struct indirect *block_1 = read_indirect (d_indirect_1);
    block_sector_t d_indirect_2 = (sectors - 123 - 128) / 128;
    int offset = (sectors - 123 - 128) % 128;
    struct indirect *block_2 = read_indirect (block_1->blocks[d_indirect_2]);
    block_sector_t sector = block_2->blocks[offset];
    free(block_1);
    free(block_2);
    return sector;
  }
  else {
    PANIC ("file size is too big");
  }
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
inode_allocate (struct inode_disk *disk_inode, size_t start_sector, size_t sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  size_t index = 0;
  
  if (sectors > MAX_SECTOR_SIZE) PANIC ("file size is too big");
  
  if (sectors > 0 && start_sector < 123)
    {
      // up to 123 direct blocks allocate & write zeros
      size_t start_idx = start_sector;
      size_t inode_count = start_idx + sectors <= 123 ? start_idx + sectors : 123;
      for (size_t i = start_idx; i < inode_count; i++)
        {
          free_map_allocate (&disk_inode->direct[i], &index);
          cache_write (fs_device, disk_inode->direct[i], zeros, BLOCK_SECTOR_SIZE, 0);
          start_sector++;
          sectors--;
        }
    }
  if (sectors > 0 && start_sector < 123 + 128)
    {
      // up to 128 singly indirect blocks allocate & write zeros
      // s_indirect block allocate & write
      struct indirect *s_indirect = calloc (1, sizeof (struct indirect));
      if ( (int) disk_inode->s_indirect != -1) 
        cache_read (fs_device, disk_inode->s_indirect, s_indirect, BLOCK_SECTOR_SIZE, 0);

      size_t start_idx = start_sector - 123;
      size_t inode_count = start_idx + sectors <= 128 ? start_idx + sectors : 128;
      for (size_t i = start_idx; i < inode_count; i++)
        {
          free_map_allocate (&s_indirect->blocks[i], &index);
          cache_write (fs_device, s_indirect->blocks[i], zeros, BLOCK_SECTOR_SIZE, 0);
          start_sector++;
          sectors--;
        }
      if ( (int) disk_inode->s_indirect == -1) 
        free_map_allocate (&disk_inode->s_indirect, &index);
      cache_write (fs_device, disk_inode->s_indirect, s_indirect, BLOCK_SECTOR_SIZE, 0);
      free (s_indirect);
    }
  if (sectors > 0 && start_sector < 123 + 128 + 128 * 128)
    {
      // up to 128*128 doubly indirect blocks allocate & write zeros
      // up to 128 indirect blocks allocate & write
      // d_indirect block allocate & write

      struct indirect *d_indirect = calloc (1, sizeof (struct indirect));
      if ( (int) disk_inode->d_indirect != -1) 
        cache_read (fs_device, disk_inode->d_indirect, d_indirect, BLOCK_SECTOR_SIZE, 0);
      else {
        for (int i = 0; i < 128; i++)
          d_indirect->blocks[i] = -1;
      }

      size_t start_idx = start_sector - 123 - 128;
      size_t end_idx = start_idx + sectors - 1;
      size_t start_indirect_sector = start_idx / 128;
      size_t start_indirect_offset = start_idx % 128;
      size_t end_indirect_sector = end_idx / 128;
      size_t end_indirect_offset = end_idx % 128;

      for (size_t i = start_indirect_sector; i <= end_indirect_sector; i++)
        {
          struct indirect *indirect = calloc (1, sizeof (struct indirect));
          if ((int)d_indirect->blocks[i] != -1) 
            cache_read (fs_device, d_indirect->blocks[i], indirect, BLOCK_SECTOR_SIZE, 0);

          size_t start = i == start_indirect_sector ? start_indirect_offset : 0;
          size_t end = i == end_indirect_sector ? end_indirect_offset : 127;
          for (size_t j = start; j <= end; j++)
            {
              free_map_allocate (&indirect->blocks[j], &index);
              cache_write (fs_device, indirect->blocks[j], zeros, BLOCK_SECTOR_SIZE, 0);
              start_sector++;
              sectors--;
            }
          if ((int)d_indirect->blocks[i] == -1) 
            free_map_allocate (&d_indirect->blocks[i], &index);

          cache_write (fs_device, d_indirect->blocks[i], indirect, BLOCK_SECTOR_SIZE, 0);
          free (indirect);
        }
      if ( (int) disk_inode->d_indirect == -1) {
        free_map_allocate (&disk_inode->d_indirect, &index);
      }
      cache_write (fs_device, disk_inode->d_indirect, d_indirect, BLOCK_SECTOR_SIZE, 0);
      free (d_indirect);
    }
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->s_indirect = -1;
      disk_inode->d_indirect = -1;
      disk_inode->dir = dir;
      inode_allocate (disk_inode, 0, sectors);
      // disk_inode write
      cache_write (fs_device, sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
      success = true;
      free (disk_inode);
    }
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
  lock_init (&inode->inode_lock);
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  // block_read (fs_device, inode->sector, &inode->data);
  cache_read (fs_device, inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
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

bool
inode_get_removed (const struct inode *inode)
{
  return inode->removed;
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
      lock_acquire (&inode->inode_lock);
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          // PANIC ("free_map_release todo\n");
          // Todo
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
        }
      lock_release (&inode->inode_lock);
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

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = new_byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_read (fs_device, sector_idx, buffer + bytes_read, chunk_size, sector_ofs);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

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

  if (inode->deny_write_cnt)
    return 0;
  
  if (size <= 0)
    return 0;

  if ((int)new_byte_to_sector (inode, offset + size - 1) == -1) {
    // PANIC ("file growth needed\n");
    off_t old_len = inode->data.length;
    off_t new_len = offset + size;
    size_t start_sector = bytes_to_sectors (old_len);
    size_t end_sector = bytes_to_sectors (new_len);
    if (start_sector < end_sector) {
      inode_allocate (&inode->data, start_sector, end_sector - start_sector);
    }

    inode->data.length = new_len;
    cache_write (fs_device, inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = new_byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_write (fs_device, sector_idx, (void *) buffer + bytes_written, chunk_size, sector_ofs);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

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

/* Returns if inode disk is direcotry or not*/
bool
inode_dir (const struct inode *inode)
{
  return inode->data.dir;
}
