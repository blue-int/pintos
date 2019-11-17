#include <hash.h>
#include <stdio.h>
#include "vm/swap.h"
#include "devices/block.h"

static struct hash swap_table;
static unsigned swap_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool swap_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

void swap_init (void) {
  hash_init (&swap_table, swap_hash_func, swap_less_func, NULL);
}

bool swap_out (struct fte * fte) {
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  // printf("hiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii\n");
  // block_write (swap_block, 1, fte->vaddr);
  return true;
}

void swap_in (void *vaddr) {

}

static unsigned swap_hash_func (const struct hash_elem *e, void *aux UNUSED) {
  struct fte *fte = hash_entry (e, struct fte, hash_elem);
  return hash_int((int)(fte->paddr));
}

static bool swap_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct fte *fte_a = hash_entry (a, struct fte, hash_elem);
  struct fte *fte_b = hash_entry (b, struct fte, hash_elem);
  return fte_a->paddr < fte_b->paddr;
}
