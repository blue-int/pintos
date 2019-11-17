#include "threads/thread.h"
#include "vm/swap.h"
#include <stdio.h>

static struct hash st;
static unsigned swap_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool swap_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static int index = 0;
void swap_init (void) {
  hash_init (&st, swap_hash_func, swap_less_func, NULL);
}

bool swap_out (struct fte *fte) {
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  block_sector_t block_index = index;
  struct thread *cur = thread_current ();
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write (swap_block, block_index + i, fte->paddr + i * BLOCK_SECTOR_SIZE);
    index++;
  }
  pagedir_clear_page (cur->pagedir, fte->vaddr);
  swap_insert (fte->vaddr, block_index);
  return true;
}

bool swap_check (void *vaddr) {
  struct ste sample;
  sample.vaddr = vaddr;
  struct hash_elem *e = hash_find (&st, &sample.hash_elem);
  if (e == NULL)
    return false;
  else
    return true;
}

void swap_in (void *vaddr) {
  struct ste sample;
  sample.vaddr = vaddr;
  struct hash_elem *e = hash_find (&st, &sample.hash_elem);
  struct ste *ste = hash_entry (e, struct ste, hash_elem);
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = ft_allocate (PAL_USER);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read (swap_block, ste->block_index + i, kpage + i * BLOCK_SECTOR_SIZE);
  }
  struct thread *cur = thread_current ();
  pagedir_set_page (cur->pagedir, vaddr, kpage, true);
}

void swap_insert (void *vaddr, block_sector_t block_index) {
  struct ste *ste = (struct ste *) malloc (sizeof(struct ste));
  ste->vaddr = vaddr;
  ste->block_index = block_index;
  hash_insert (&st, &ste->hash_elem);
}

static unsigned swap_hash_func (const struct hash_elem *e, void *aux UNUSED) {
  struct ste *ste = hash_entry (e, struct ste, hash_elem);
  return hash_int((int)(ste->vaddr));
}

static bool swap_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct ste *ste_a = hash_entry (a, struct ste, hash_elem);
  struct ste *ste_b = hash_entry (b, struct ste, hash_elem);
  return ste_a->vaddr < ste_b->vaddr;
}
