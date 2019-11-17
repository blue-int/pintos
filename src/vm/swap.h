#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

struct ste {
  void * vaddr;
  block_sector_t block_index;
  struct hash_elem hash_elem;
};

void swap_init (void);
bool swap_out (struct fte * fte);
void swap_in (void *vaddr);
bool swap_check (void *vaddr);
void swap_insert (void *vaddr, block_sector_t block_index);

#endif
