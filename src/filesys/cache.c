#include "threads/thread.h"
#include "filesys/cache.h"
#include <stdio.h>
#include <string.h>

static struct list buffer_cache;
static struct lock cache_lock;

void cache_init (void) {
  lock_init (&cache_lock);
  list_init (&buffer_cache);
  for (int i = 0; i < 64; i++) {
    struct bce *bce = (struct bce *) malloc (sizeof (struct bce));
    bce->valid = false;
    bce->dirty = false;
    bce->acc_cnt = 0;
    bce->sector = -1;
    list_push_back (&buffer_cache, &bce->list_elem);
  }
}

struct bce * cache_allocate (struct block *block) {
  struct bce *evict_target = NULL;
  int acc_cnt = -1;
  struct list_elem *e;

  for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache); e = list_next (e)) {
    struct bce *bce = list_entry (e, struct bce, list_elem);
    if (bce->valid == false) {
      return bce;
    }
    if (acc_cnt == -1 || acc_cnt > bce->acc_cnt) {
      evict_target = bce;
      acc_cnt = bce->acc_cnt;
    }
  }

  ASSERT (evict_target != NULL);
  ASSERT (evict_target->acc_cnt != -1);
  ASSERT (evict_target->valid == 1);

  if (evict_target->dirty) 
    block_write (block, evict_target->sector, evict_target->buffer);

  return evict_target;
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer, int size, int offset)
{
  lock_acquire (&cache_lock);

  struct bce *target = cache_find (block, sector);
  memcpy (buffer, target->buffer + offset, size);
  target->acc_cnt += 1;

  lock_release (&cache_lock);
}

void
cache_write (struct block *block, block_sector_t sector, void *buffer, int size, int offset)
{ 
  lock_acquire (&cache_lock);

  struct bce *target = cache_find (block, sector);
  memcpy (target->buffer + offset, buffer, size);
  target->dirty = true;

  lock_release (&cache_lock);
}

struct bce *cache_find (struct block *block, block_sector_t sector) {
  struct list_elem *e;
  struct bce *bce = NULL;
  for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache); e = list_next (e)) {
    bce = list_entry (e, struct bce, list_elem);
    if (bce->sector == sector) {
      return bce;
    }
  }
  bce = cache_allocate (block);
  block_read (block, sector, bce->buffer);
  bce->valid = true;
  bce->dirty = false;
  bce->acc_cnt = 0;
  bce->sector = sector;
  return bce;
}

void cache_flush (struct block *block) {
  struct list_elem *e;
  struct bce *bce = NULL;
  for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache); e = list_next (e)) {
    bce = list_entry (e, struct bce, list_elem);
    if (bce->dirty) {
      block_write (block, bce->sector, bce->buffer);
      bce->dirty = false;
    }
  }
}