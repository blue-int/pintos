#include <bitmap.h>
#include "threads/thread.h"
#include "vm/swap.h"
#include <stdio.h>

static struct bitmap *slot_list;

void swap_init (void) {
  slot_list = bitmap_create (8192);
}

bool swap_out (struct hash *spt, void *vaddr, void *paddr) {
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  size_t block_index = bitmap_scan_and_flip (slot_list, 0, PGSIZE / BLOCK_SECTOR_SIZE, false);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++){
    block_write (swap_block, block_index + i, paddr + i * BLOCK_SECTOR_SIZE);
  }
  struct spte *spte = spt_find (spt, vaddr);
  ASSERT (spte != NULL);
  spte->block_index = block_index;
  return true;
}

void swap_in (struct hash *spt, void *_vaddr) {
  void *vaddr = pg_round_down(_vaddr);
  struct spte *spte = spt_find (spt, vaddr);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = ft_allocate (PAL_USER, vaddr);
  bitmap_scan_and_flip (slot_list, spte->block_index, PGSIZE / BLOCK_SECTOR_SIZE, true);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read (swap_block, (spte->block_index) + i, kpage + (i * BLOCK_SECTOR_SIZE));
  spte->paddr = kpage;
  spte->status = ON_FRAME;
  pagedir_set_page (thread_current ()->pagedir, vaddr, kpage, spte->writable);
  pagedir_set_accessed (thread_current ()->pagedir, vaddr, false);
  pagedir_set_dirty (thread_current ()->pagedir, vaddr, spte->dirty);
  pagedir_set_dirty (thread_current ()->pagedir, kpage, spte->dirty);
  ft_set_pin (kpage, false);
}

void swap_remove (block_sector_t block_index) {
  bitmap_scan_and_flip (slot_list, block_index, PGSIZE / BLOCK_SECTOR_SIZE, true);
}
