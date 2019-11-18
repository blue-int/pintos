#include "vm/page.h"

void spt_init (struct hash *spt) {
  hash_init (spt, spt_hash_func, spt_less_func, NULL);
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

void spt_insert (void *vaddr, void *paddr) {
  struct thread *cur = thread_current ();
  struct spte *spte = (struct spte *) malloc (sizeof(struct spte));
  spte->vaddr = vaddr;
  hash_insert (&cur->spt, &spte->hash_elem);
  ft_add_vaddr (vaddr, paddr);
}

void spt_delete (struct hash *spt, void *vaddr) {
  struct spte p;
  p.vaddr = vaddr;
  struct hash_elem *e = hash_delete (spt, &p.hash_elem);
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  free (spte);
}

void spt_destroy (struct hash *spt) {
  if (spt != NULL) {
    hash_apply (spt, fte_remove);
    hash_destroy (spt, NULL);
  }
}
