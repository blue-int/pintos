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
    bce->sector = -1;
    bce->acc_cnt = 0;
    bce->valid = false;
    bce->dirty = false;
    lock_init(&bce->bce_lock);
    list_push_back (&buffer_cache, &bce->list_elem);
  }
}

struct bce * cache_allocate (void) {
  struct bce *empty_target = NULL;
  struct bce *evict_target = NULL;
  int acc_cnt = -1;
  struct list_elem *e;

  lock_acquire (&cache_lock);
  for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache); e = list_next (e)) {
    struct bce *bce = list_entry (e, struct bce, list_elem);
    if (bce->valid == false) {
      empty_target = bce;
    }
    if (acc_cnt < bce->acc_cnt) {
      evict_target = bce;
    }
  }

  if (empty_target == NULL) {
    if (evict_target == NULL)
      PANIC ("Evict target cannot be found\n");
    ASSERT (evict_target->acc_cnt != -1);
    ASSERT (evict_target->valid == 1);
    lock_acquire (&evict_target->bce_lock);
    lock_release (&cache_lock);
    struct block *filesys_block = block_get_role (BLOCK_FILESYS);
    block_write (filesys_block, evict_target->sector, evict_target->buffer);
    cache_clear (evict_target);
    lock_release (&evict_target->bce_lock);
    return evict_target;
  }
  else {
    empty_target->valid = true;
    lock_release (&cache_lock);
    return empty_target;
  }
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer, int size, int offset)
{ 
  struct bce *target = cache_find (sector);
  
  if (target == NULL) {
    target = cache_allocate ();
    lock_acquire (&target->bce_lock);
    block_read (block, sector, target->buffer);
  }
  else
    printf("target has been found\n");
  printf("%d is target sector and %d is sector_idx\n", target->sector, sector);
  // target->sector = sector;

  if (!lock_held_by_current_thread (&target->bce_lock))
    lock_acquire (&target->bce_lock);

  if (offset == 0 && size == BLOCK_SECTOR_SIZE)
    memcpy (buffer, target->buffer, size);
  else 
    memcpy (buffer, target->buffer + offset, size);

  target->acc_cnt += 1;
  lock_release (&target->bce_lock);
}

void
cache_write (struct block *block, block_sector_t sector, void *buffer, int size, int offset)
{ 
  struct bce *target = cache_find (sector);

  if (target == NULL) {
    target = cache_allocate ();
    lock_acquire (&target->bce_lock);
    block_read (block, sector, target->buffer);
  }

  if (!lock_held_by_current_thread (&target->bce_lock))
    lock_acquire (&target->bce_lock);

  target->dirty = true;
  if (offset == 0 && size == BLOCK_SECTOR_SIZE)
    memcpy (target->buffer, buffer, size);
  else 
    memcpy (target->buffer + offset, buffer, size);

  // block_write (block, sector, target->buffer);
  lock_release (&target->bce_lock);

}

struct bce *cache_find (block_sector_t sector) {
  struct list_elem *e;
  for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache); e = list_next (e)) {
    struct bce *bce = list_entry (e, struct bce, list_elem);
    if (bce->sector == sector) {
      return bce;
    }
  }
  return NULL;
}

void cache_clear (struct bce *bce) {
  bce->acc_cnt = 0;
  bce->sector = -1;
  bce->valid = false;
  bce->dirty = false;
}

