#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

void swap_init (void);
bool swap_out (struct hash *spt, void *vaddr, void *paddr);
void swap_in (struct hash *spt, void *vaddr);
void swap_remove (block_sector_t block_index);

#endif
