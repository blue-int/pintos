#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>

struct spte {
  struct hash_elem hash_elem;
  void* va;
};

void spt_init (struct hash *spt);
unsigned spt_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void spt_insert (void *vaddr);
void spt_delete (void *vaddr);
void spt_destroy (void);

#endif
