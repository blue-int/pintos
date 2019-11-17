#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
#include "devices/block.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/frame.h"

struct spte {
  struct hash_elem hash_elem;
  void *vaddr;
  void *paddr;
  block_sector_t block_index;
};

void spt_init (struct hash *spt);
unsigned spt_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void spt_insert (void *vaddr, void *paddr);
void spt_delete (struct hash *spt, void *vaddr);
void spt_destroy (struct hash *spt);

#endif
