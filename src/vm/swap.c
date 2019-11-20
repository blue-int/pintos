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

bool swap_out (struct hash *spt, struct fte *fte) {
  lock_acquire (&swap_out_lock);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  size_t block_index = bitmap_scan_and_flip (slot_list, 0, PGSIZE / BLOCK_SECTOR_SIZE, false);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++){
    block_write (swap_block, block_index + i, fte->paddr + i * BLOCK_SECTOR_SIZE);
  }
  swap_insert (spt, fte->vaddr, block_index);
  lock_release (&swap_out_lock);
  return true;
}

bool swap_check (struct hash *spt, void *vaddr) {
  struct spte sample;
  sample.vaddr = pg_round_down(vaddr);
  struct hash_elem *e = hash_find (spt, &sample.hash_elem);
  if (e == NULL)
    return false;
  else
    return true;
}

void swap_in (struct hash *spt, void *_vaddr) {
  lock_acquire (&swap_in_lock);
  struct spte sample;
  void *vaddr = pg_round_down(_vaddr);
  sample.vaddr = vaddr;
  struct hash_elem *e = hash_find (spt, &sample.hash_elem);
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = ft_allocate (PAL_USER, vaddr);
  bitmap_scan_and_flip (slot_list, spte->block_index, PGSIZE / BLOCK_SECTOR_SIZE, true);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read (swap_block, (spte->block_index) + i, kpage + (i * BLOCK_SECTOR_SIZE));
  spte->status = FRAME;
  spte->paddr = kpage;
  pagedir_set_page (thread_current ()->pagedir, vaddr, kpage, spte->writable);
  pagedir_set_accessed (thread_current ()->pagedir, vaddr, false);
  ft_set_pin (kpage, false);
  lock_release (&swap_in_lock);
}

void swap_remove (block_sector_t block_index) {
  bitmap_scan_and_flip (slot_list, block_index, PGSIZE / BLOCK_SECTOR_SIZE, true);
}

void swap_insert (struct hash *spt, void *vaddr, block_sector_t block_index) {
  struct spte sample;
  sample.vaddr = vaddr;
  struct hash_elem *e = hash_find (spt, &sample.hash_elem);
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  spte->status = SWAP;
  spte->block_index = block_index;
  spte->paddr = NULL;
}
