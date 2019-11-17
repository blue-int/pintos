#include "threads/thread.h"
#include "vm/swap.h"
#include <stdio.h>

static int index = 0;

bool swap_out (struct hash *spt, struct fte *fte) {
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  block_sector_t block_index = index;
  // struct thread *cur = thread_current ();
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write (swap_block, block_index + i, fte->paddr + i * BLOCK_SECTOR_SIZE);
    index++;
  }
  // pagedir_clear_page (cur->pagedir, fte->vaddr);
  // printf ("swap out vaddr: %p\n", fte->vaddr);
  swap_insert (spt, fte->vaddr, block_index);
  return true;
}

bool swap_check (struct hash *spt, void *vaddr) {
  struct spte sample;
  sample.vaddr = pg_round_down(vaddr);
  // printf ("vaddr: %p\n", vaddr);
  // printf ("page number is: %p\n", pg_round_down(vaddr));
  struct hash_elem *e = hash_find (spt, &sample.hash_elem);
  // printf ("hash elem is: %p\n", e);
  if (e == NULL)
    return false;
  else
    return true;
}

void swap_in (struct hash *spt, void *_vaddr) {
  // printf ("swap in begin\n");
  struct spte sample;
  void *vaddr = pg_round_down(_vaddr);
  sample.vaddr = vaddr;
  // printf ("vaddr: %p\n", vaddr);
  struct hash_elem *e = hash_find (spt, &sample.hash_elem);
  // printf ("hash elem is: %p\n", e);
  struct spte *spte = hash_entry (e, struct spte, hash_elem);  
  // printf ("spte is: %p\n", spte);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = ft_allocate (PAL_USER);
  // printf ("kpage is: %p\n", kpage);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read (swap_block, (spte->block_index) + i, kpage + (i * BLOCK_SECTOR_SIZE));
    // printf(" %dth block has been read\n", i);
  }
  // printf ("swap in point5\n");
  struct thread *cur = thread_current ();
  pagedir_set_page (cur->pagedir, vaddr, kpage, true);
  spt_delete (spt, vaddr);
  // printf ("swap in completed\n");
}

void swap_insert (struct hash *spt, void *vaddr, block_sector_t block_index) {
  struct spte *spte = (struct spte *) malloc (sizeof(struct spte));
  spte->vaddr = vaddr;
  spte->block_index = block_index;
  hash_insert (spt, &spte->hash_elem);
}
