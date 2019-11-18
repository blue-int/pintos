#include "vm/page.h"
#include <stdio.h>

struct hash * spt_init (void) {
  struct hash *spt = (struct hash *) malloc (sizeof (struct hash));
  hash_init (spt, spt_hash_func, spt_less_func, NULL);
  return spt;
}

unsigned spt_hash_func (const struct hash_elem *e, void *aux UNUSED) {
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  return hash_int((int)(spte->vaddr));
}

bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct spte *spte_a = hash_entry (a, struct spte, hash_elem);
  struct spte *spte_b = hash_entry (b, struct spte, hash_elem);
  return spte_a->vaddr < spte_b->vaddr;
}

void spt_insert (void *vaddr, void *paddr, bool writable) {
  struct thread *cur = thread_current ();
  struct spte *spte = (struct spte *) malloc (sizeof(struct spte));
  spte->vaddr = vaddr;
  spte->paddr = paddr;
  spte->status = FRAME;
  spte->writable = writable;
  hash_insert (cur->spt, &spte->hash_elem);
}

void spt_remove (struct hash_elem *e, void *aux UNUSED) {
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  if (spte->paddr != NULL)
    fte_remove (spte->paddr);
  else if (spte->status == SWAP)
    swap_remove (spte->block_index);
  free (spte);
}

void spt_destroy (struct hash *spt) {
  if (spt != NULL) {
    hash_destroy (spt, spt_remove);
  }
}
