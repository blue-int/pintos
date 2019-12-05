#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H
#include <debug.h>
#include "threads/synch.h"
#include "devices/block.h"

struct bce {
  block_sector_t sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];
  int acc_cnt;
  bool valid;
  bool dirty;
  struct lock bce_lock;
  struct list_elem list_elem;
};

void cache_init (void);
void cache_clear (struct bce *bce);
void cache_read (struct block *block, block_sector_t sector, void *buffer, int size, int offset);
void cache_write (struct block *block, block_sector_t sector, void *buffer, int size, int offset);

struct bce *cache_allocate (void);
struct bce *cache_find (block_sector_t sector);

#endif
