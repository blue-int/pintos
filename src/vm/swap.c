#include <bitmap.h>
#include "threads/thread.h"
#include "vm/swap.h"
#include <stdio.h>

static struct bitmap *slot_list;
struct lock swap_in_lock;
struct lock swap_out_lock;

void swap_init (void) {
  slot_list = bitmap_create (8192);
  lock_init (&swap_in_lock);
  lock_init (&swap_out_lock);
}

bool swap_out (struct hash *spt, struct fte *fte, bool dirty) {
  lock_acquire (&swap_out_lock);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  size_t block_index = bitmap_scan_and_flip (slot_list, 0, PGSIZE / BLOCK_SECTOR_SIZE, false);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++){
    block_write (swap_block, block_index + i, fte->paddr + i * BLOCK_SECTOR_SIZE);
  }
  swap_insert (spt, fte->vaddr, block_index, dirty);
  lock_release (&swap_out_lock);
  return true;
}

void swap_in (struct hash *spt, void *_vaddr) {
  lock_acquire (&swap_in_lock);
  void *vaddr = pg_round_down(_vaddr);
  struct spte *spte = spt_find (spt, vaddr);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = ft_allocate (PAL_USER, vaddr);
  bitmap_scan_and_flip (slot_list, spte->block_index, PGSIZE / BLOCK_SECTOR_SIZE, true);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read (swap_block, (spte->block_index) + i, kpage + (i * BLOCK_SECTOR_SIZE));
  spte->status = ON_FRAME;
  spte->paddr = kpage;
  pagedir_set_page (thread_current ()->pagedir, vaddr, kpage, spte->writable);
  pagedir_set_accessed (thread_current ()->pagedir, vaddr, false);
  pagedir_set_dirty (thread_current ()->pagedir, vaddr, false);
  ft_set_pin (kpage, false);
  lock_release (&swap_in_lock);
}

void swap_remove (block_sector_t block_index) {
  bitmap_scan_and_flip (slot_list, block_index, PGSIZE / BLOCK_SECTOR_SIZE, true);
}

void swap_insert (struct hash *spt, void *vaddr, block_sector_t block_index, bool dirty) {
  struct spte *spte = spt_find (spt, vaddr);
  spte->status = ON_SWAP;
  spte->block_index = block_index;
  spte->paddr = NULL;
  spte->dirty = dirty;
}
