#include "threads/thread.h"
#include "filesys/cache.h"
#include <stdio.h>

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
    lock_release (&cache_lock);
    return empty_target;
  }
}

// struct bce * cache_evict (void) {
//   struct bce *target = NULL;
//   int acc_cnt = -1;
//   struct list_elem *e;
//   for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache); e = list_next (e)) {
//     struct bce *bce = list_entry (e, struct bce, list_elem);
//     if (acc_cnt > bce->acc_cnt) {
//       target = bce;
//     }
//   }
//   if (target == NULL)
//     PANIC ("BCE cannot be found\n");
//   ASSERT (target->acc_cnt != -1);
//   ASSERT (target->valid == 1);
//   lock_acquire (&target->bce_lock);
//   struct block *filesys_block = block_get_role (BLOCK_FILESYS);
//   block_write (filesys_block, target->sector, target->buffer);
//   cache_clear (target);
//   lock_release (&target->bce_lock);
//   return target;
// }

void cache_clear (struct bce *bce) {
  bce->acc_cnt = 0;
  bce->sector = -1;
  bce->valid = false;
  bce->dirty = false;
}

// void fte_remove (void *paddr) {
//   lock_acquire (&ft_lock);
//   struct fte sample;
//   sample.paddr = paddr;
//   struct hash_elem *elem = hash_delete (&ft_hash, &sample.hash_elem);
//   struct fte *fte = hash_entry (elem, struct fte, hash_elem);
//   ASSERT (fte != NULL);
//   list_remove (&fte->list_elem);
//   free (fte);
//   lock_release (&ft_lock);
// }
